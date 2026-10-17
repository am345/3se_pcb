#include "math.h"
#include "fdcan.h"
#include "CAN_driver.h"
#include "power_calculation.h"

// ====================== Private Variables ======================
static uint8_t CAN_RXbuff[8];
static uint8_t CAN_TXbuff[8];
volatile uint16_t can_target_power_lim = 100;	// Initial power limit for bench testing; CAN overrides at runtime.

// ====================== Health Monitoring ======================
volatile CAN_Health_t can_health = {0};
static volatile uint32_t last_rx_tick = 0;
static volatile uint32_t last_tx_ok_tick = 0;

static int16_t float_to_i16_saturated(float value)
{
	if (value <= -32768.0f) return -32768;
	if (value >= 32767.0f) return 32767;
	return (int16_t)value;
}

// ====================== Internal: Full FDCAN Reinit ======================
/**
 * @brief  Performs a complete FDCAN2 stop -> deinit -> init -> filter -> start cycle.
 *         Called from task context (NOT from ISR) to recover from Bus-Off or silent failures.
 */
static void FDCAN_FullReinit(void)
{
	// Recovery runs in main context. Mask the FDCAN IRQ while the peripheral
	// and its message RAM are being reset; the 20kHz control IRQ is unaffected.
	HAL_NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
	HAL_NVIC_ClearPendingIRQ(FDCAN2_IT0_IRQn);

	// [UPGRADE] RCC hardware-level reset: equivalent to pressing the physical RESET button
	// but ONLY for the FDCAN peripheral. Clears ALL internal state including protocol engine,
	// Message RAM controller, and error latches that HAL_FDCAN_DeInit() cannot reach.
	// This replaces the old HAL_FDCAN_Stop + HAL_FDCAN_DeInit which only did a software-level cleanup.
	__HAL_RCC_FDCAN_CLK_ENABLE();         // Ensure clock is on (required for reset to propagate)
	__HAL_RCC_FDCAN_FORCE_RESET();        // Pull FDCAN hardware reset line LOW
	__HAL_RCC_FDCAN_RELEASE_RESET();      // Release — hardware is now factory-fresh
	
	// Reset HAL software state so HAL_FDCAN_Init() will call MspInit (re-enables GPIO + NVIC)
	hfdcan2.State = HAL_FDCAN_STATE_RESET;
	
	// Re-initialize with graceful error handling (DO NOT call MX_FDCAN2_Init — it has Error_Handler!)
	hfdcan2.Instance = FDCAN2;
	hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
	hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
	hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
	hfdcan2.Init.AutoRetransmission = ENABLE;
	hfdcan2.Init.TransmitPause = ENABLE;
	hfdcan2.Init.ProtocolException = DISABLE;
	hfdcan2.Init.NominalPrescaler = 17;
	hfdcan2.Init.NominalSyncJumpWidth = 4;
	hfdcan2.Init.NominalTimeSeg1 = 5;
	hfdcan2.Init.NominalTimeSeg2 = 4;
	hfdcan2.Init.DataPrescaler = 17;
	hfdcan2.Init.DataSyncJumpWidth = 4;
	hfdcan2.Init.DataTimeSeg1 = 5;
	hfdcan2.Init.DataTimeSeg2 = 4;
	hfdcan2.Init.StdFiltersNbr = 28;
	hfdcan2.Init.ExtFiltersNbr = 8;
	hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
	
	if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK) {
		// Leave needs_reinit set and retry later from CAN_Service().
		can_health.error_count++;
		return;
	}

	// HAL_FDCAN_Init() calls MspInit(), which enables the IRQ. Keep it masked
	// until filter/start/notification setup has completed atomically.
	HAL_NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
	if (FDCAN_FilterInit() != HAL_OK || FDCAN_start() != HAL_OK) {
		// The helpers already counted the concrete error. A later service pass
		// will perform another full hardware reset before retrying.
		return;
	}

	can_health.reinit_count++;
	can_health.needs_reinit = 0;
	can_health.bus_active = 0;
	can_health.tx_fail_count = 0;
	HAL_NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
}

// ====================== RX Callback (ISR Context) ======================
/**
 * @brief  CAN Receive Callback. Validates interrupt source, checks HAL return,
 *         and extracts power limit from the correct byte pair.
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
	// Handle FIFO message lost event (overflow)
	if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST)
	{
		can_health.fifo_overflow_count++;
		return;
	}

	// Handle FIFO full warning
	if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_FULL)
	{
		can_health.fifo_overflow_count++;
		// Don't return - still try to read the message below
	}

	// Only process actual new messages
	if (!(RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE))
		return;

	FDCAN_RxHeaderTypeDef RxHeader;

	if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, CAN_RXbuff) != HAL_OK)
	{
		can_health.error_count++;
		return;
	}

	// Successful reception
	can_health.rx_count++;
	last_rx_tick = HAL_GetTick();
	can_health.bus_active = 1;

	// Reject malformed/short frames before reading Byte[4:5]. The hardware
	// filter normally enforces the ID, but callback validation keeps stale RX
	// buffer bytes from being reused if a short frame reaches this path.
	if (RxHeader.IdType == FDCAN_STANDARD_ID &&
		RxHeader.RxFrameType == FDCAN_DATA_FRAME &&
		RxHeader.Identifier == RECEIVE_ID &&
		RxHeader.DataLength >= FDCAN_DLC_BYTES_6)
	{
		// Extract the target power limit from payload byte array
		uint16_t raw_power = (uint16_t)(CAN_RXbuff[RECEIVE_NUM * 2] << 8 | CAN_RXbuff[RECEIVE_NUM * 2 + 1]);

		// Enforce minimum 40W; otherwise apply 1W safety margin
		if (raw_power < 40)
			can_target_power_lim = 40;
		else
			can_target_power_lim = raw_power - 1;
	}
}

// ====================== Error Status Callback (ISR Context) ======================
/**
 * @brief  Called by HAL when FDCAN protocol error status changes (Bus-Off, Error Passive, etc.)
 *         Sets reinit flag for deferred recovery in task context.
 */
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
	can_health.error_count++;
	can_health.tx_error_active = 1u;

	if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
	{
		can_health.busoff_count++;
		can_health.needs_reinit = 1;		// Defer heavy reinit to task context
	}
}

// ====================== Generic Error Callback (ISR Context) ======================
/**
 * @brief  Called by HAL on FDCAN peripheral errors (RAM access, timeout, etc.)
 */
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
	can_health.error_count++;
	can_health.tx_error_active = 1u;

	// If error is severe enough, request reinit
	uint32_t err = HAL_FDCAN_GetError(hfdcan);
	if (err != HAL_FDCAN_ERROR_NONE)
	{
		can_health.needs_reinit = 1;
	}
}

// ====================== CAN Transmission ======================
/**
 * @brief  Transmit a CAN frame. Checks return value and tracks failures.
 *         If consecutive failures exceed threshold, requests FDCAN reinit.
 */
void FCAN_transmit(FDCAN_HandleTypeDef *hfdcan, uint32_t can_id, uint32_t DataLength, uint8_t tx_data[])
{
	FDCAN_TxHeaderTypeDef TxHeader;

	// Do not touch FDCAN from the 1kHz timer ISR while a reset/reconfigure is
	// pending in main context.
	if (can_health.needs_reinit) {
		return;
	}

	TxHeader.Identifier = can_id;
	TxHeader.IdType = FDCAN_STANDARD_ID;
	TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	TxHeader.DataLength = DataLength;
	TxHeader.ErrorStateIndicator = FDCAN_ESI_PASSIVE;
	TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
	TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
	TxHeader.TxEventFifoControl = FDCAN_STORE_TX_EVENTS;
	TxHeader.MessageMarker = 0;

	if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &TxHeader, tx_data) == HAL_OK)
	{
		can_health.tx_enqueue_count++;
		can_health.tx_fail_count = 0;	// Reset consecutive failure counter
	}
	else
	{
		can_health.tx_fail_count++;
		if (can_health.tx_fail_count >= CAN_TX_FAIL_REINIT_THR)
		{
			can_health.needs_reinit = 1;
		}
	}
}

void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxEventFifoITs)
{
	FDCAN_TxEventFifoTypeDef tx_event;
	uint32_t now = HAL_GetTick();

	if (TxEventFifoITs & (FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST)) {
		can_health.error_count++;
		can_health.tx_error_active = 1u;
	}

	if (!(TxEventFifoITs & FDCAN_IT_TX_EVT_FIFO_NEW_DATA)) {
		return;
	}

	while ((hfdcan->Instance->TXEFS & FDCAN_TXEFS_EFFL) != 0u) {
		if (HAL_FDCAN_GetTxEvent(hfdcan, &tx_event) != HAL_OK) {
			can_health.error_count++;
			can_health.tx_error_active = 1u;
			break;
		}
		can_health.tx_count++;
		last_tx_ok_tick = now;
		can_health.tx_error_active = 0u;
	}
}

uint8_t CAN_RxRecent(void)
{
	if (can_health.rx_count == 0u || can_health.bus_active == 0u || can_health.needs_reinit) {
		return 0u;
	}

	return ((HAL_GetTick() - last_rx_tick) <= CAN_WDG_TIMEOUT_MS) ? 1u : 0u;
}

uint8_t CAN_TxRecent(void)
{
	if (can_health.tx_count == 0u ||
	    can_health.tx_fail_count != 0u ||
	    can_health.tx_error_active ||
	    can_health.needs_reinit) {
		return 0u;
	}

	return ((HAL_GetTick() - last_tx_ok_tick) <= CAN_WDG_TIMEOUT_MS) ? 1u : 0u;
}

// ====================== Telemetry Transmission ======================
/**
 * @brief  Packs and sends capacitor voltage, power limit, and power data to chassis.
 *         Called every 10ms from Debug_task.
 */
void power_send(void)
{
	// Telemetry frame (ID=0x211, 8 bytes, little-endian int16):
	// Byte[0:1] = input power in.P_now (W)
	// Byte[2:3] = chassis output power out.P_now (W)
	// Byte[4:5] = capacitor voltage cap.U_now * 100 (V)
	// Byte[6:7] = actual capacitor power cap.P_now (W), positive=charge, negative=discharge
	int16_t input_power = float_to_i16_saturated(in.P_now);
	int16_t output_power = float_to_i16_saturated(out.P_now);
	int16_t cap_voltage_x100 = float_to_i16_saturated(cap.U_now * 100.0f);
	int16_t cap_actual_power = float_to_i16_saturated(cap.P_now);
	uint16_t input_power_raw = (uint16_t)input_power;
	uint16_t output_power_raw = (uint16_t)output_power;
	uint16_t cap_voltage_raw = (uint16_t)cap_voltage_x100;
	uint16_t cap_actual_power_raw = (uint16_t)cap_actual_power;

	CAN_TXbuff[0] = (uint8_t)input_power_raw;
	CAN_TXbuff[1] = (uint8_t)(input_power_raw >> 8);
	CAN_TXbuff[2] = (uint8_t)output_power_raw;
	CAN_TXbuff[3] = (uint8_t)(output_power_raw >> 8);
	CAN_TXbuff[4] = (uint8_t)cap_voltage_raw;
	CAN_TXbuff[5] = (uint8_t)(cap_voltage_raw >> 8);
	CAN_TXbuff[6] = (uint8_t)cap_actual_power_raw;
	CAN_TXbuff[7] = (uint8_t)(cap_actual_power_raw >> 8);

	// Transmit via FDCAN2
	FCAN_transmit(&hfdcan2, SEND_ID, FDCAN_DLC_BYTES_8, CAN_TXbuff);
}

// ====================== Filter Configuration ======================
/**
 * @brief  Configures FDCAN2 RX filter to ONLY accept our target RECEIVE_ID.
 *         Uses mask mode with exact match (all 11 bits), rejecting everything else.
 *         This drastically reduces FIFO pressure compared to the old 0x000~0x7FF range filter.
 */
HAL_StatusTypeDef FDCAN_FilterInit(void)
{
	FDCAN_FilterTypeDef sFilterConfig;

	sFilterConfig.IdType = FDCAN_STANDARD_ID;
	sFilterConfig.FilterIndex = 0;
	sFilterConfig.FilterType = FDCAN_FILTER_MASK;
	sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	sFilterConfig.FilterID1 = RECEIVE_ID;	// Target ID to accept
	sFilterConfig.FilterID2 = 0x7FF;		// Mask: all 11 bits must match (exact match)

	if (HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig) != HAL_OK)
	{
		can_health.error_count++;
		can_health.needs_reinit = 1;
		return HAL_ERROR;
	}

	// Reject all non-matching standard frames and all remote frames
	if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT,
			FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
	{
		can_health.error_count++;
		can_health.needs_reinit = 1;
		return HAL_ERROR;
	}

	return HAL_OK;
}

// ====================== Start FDCAN ======================
/**
 * @brief  Starts FDCAN2 peripheral and activates all necessary interrupt notifications:
 *         - RX FIFO0 new message (for receiving power limits)
 *         - RX FIFO0 full + message lost (for overflow detection)
 *         - Bus-Off + Error Passive + Error Warning (for auto-recovery)
 */
HAL_StatusTypeDef FDCAN_start(void)
{
	if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
	{
		can_health.error_count++;
		can_health.needs_reinit = 1;
		return HAL_ERROR;
	}

	// Activate RX and TX event notifications
	if (HAL_FDCAN_ActivateNotification(&hfdcan2,
		FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
		FDCAN_IT_RX_FIFO0_FULL |
		FDCAN_IT_RX_FIFO0_MESSAGE_LOST |
		FDCAN_IT_TX_EVT_FIFO_NEW_DATA |
		FDCAN_IT_TX_EVT_FIFO_FULL |
		FDCAN_IT_TX_EVT_FIFO_ELT_LOST,
		0) != HAL_OK)
	{
		can_health.error_count++;
		can_health.needs_reinit = 1;
		return HAL_ERROR;
	}

	// Activate error notifications for automatic Bus-Off recovery
	if (HAL_FDCAN_ActivateNotification(&hfdcan2,
		FDCAN_IT_BUS_OFF |
		FDCAN_IT_ERROR_PASSIVE |
		FDCAN_IT_ERROR_WARNING |
		FDCAN_IT_ARB_PROTOCOL_ERROR |
		FDCAN_IT_DATA_PROTOCOL_ERROR,
		0) != HAL_OK)
	{
		can_health.error_count++;
		can_health.needs_reinit = 1;
		return HAL_ERROR;
	}

	return HAL_OK;
}

// ====================== Watchdog Check (Task Context) ======================
/**
 * @brief  Lightweight 1ms ISR-side timeout check. It only sets a recovery
 *         request; reset/reinitialization is performed by CAN_Service() from
 *         main context.
 */
void CAN_WatchdogCheck(void)
{
	// RX timeout watchdog
	// Only triggers if bus was once active (at least one frame received)
	// Prevents false triggers during bench testing without a chassis
	if (!can_health.needs_reinit && can_health.bus_active)
	{
		uint32_t now = HAL_GetTick();
		if ((now - last_rx_tick) > CAN_WDG_TIMEOUT_MS)
		{
			can_health.needs_reinit = 1;
			can_health.bus_active = 0;	// Prevent continuous reinit loops
		}
	}
}

/**
 * @brief  Performs deferred FDCAN recovery from main context.
 *         Retry attempts are spaced to avoid a tight loop on a persistent
 *         hardware/configuration failure.
 */
void CAN_Service(void)
{
	static uint32_t last_reinit_attempt = 0;
	uint32_t now;

	if (!can_health.needs_reinit) {
		return;
	}

	now = HAL_GetTick();
	if ((uint32_t)(now - last_reinit_attempt) < 10u) {
		return;
	}
	last_reinit_attempt = now;

	FDCAN_FullReinit();
	if (!can_health.needs_reinit) {
		last_rx_tick = HAL_GetTick();
	}
}
