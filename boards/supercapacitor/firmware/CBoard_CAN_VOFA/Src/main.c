#include "main.h"
#include "can.h"
#include "gpio.h"
#include "usart.h"

#include <stdint.h>
#include <stdio.h>

#define SUPERCAP_TELEMETRY_ID 0x211U
#define TELEMETRY_PERIOD_MS   10U

typedef struct {
  int16_t input_power_w;
  int16_t output_power_w;
  int16_t cap_voltage_x100;
  int16_t cap_actual_power_w;
  uint32_t rx_count;
  uint32_t last_rx_tick;
} supercap_telemetry_t;

static volatile supercap_telemetry_t g_supercap;

static void SystemClock_Config(void);
static void CAN1_FilterStart(void);
static int16_t read_i16_le(const uint8_t *data);
static void UART1_SendTelemetry(void);
static void DebugLed_Task(uint32_t now);

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  CAN1_FilterStart();

  uint32_t last_send_tick = 0;

  while (1)
  {
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_send_tick) >= TELEMETRY_PERIOD_MS)
    {
      last_send_tick = now;
      UART1_SendTelemetry();
    }
    DebugLed_Task(now);
  }
}

static void CAN1_FilterStart(void)
{
  CAN_FilterTypeDef filter = {0};

  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  // bxCAN stores an 11-bit standard ID in filter bits [31:21]. Match only
  // the 0x211 data frame so unrelated chassis traffic cannot fill FIFO0.
  filter.FilterIdHigh = (uint16_t)(SUPERCAP_TELEMETRY_ID << 5);
  filter.FilterIdLow = 0;
  filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);
  filter.FilterMaskIdLow = 0x0006U;  // Match IDE=standard and RTR=data.
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];

  if (hcan->Instance != CAN1)
  {
    return;
  }

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
  {
    return;
  }

  if (rx_header.IDE != CAN_ID_STD || rx_header.RTR != CAN_RTR_DATA ||
      rx_header.StdId != SUPERCAP_TELEMETRY_ID || rx_header.DLC < 8U)
  {
    return;
  }

  g_supercap.input_power_w = read_i16_le(&rx_data[0]);
  g_supercap.output_power_w = read_i16_le(&rx_data[2]);
  g_supercap.cap_voltage_x100 = read_i16_le(&rx_data[4]);
  g_supercap.cap_actual_power_w = read_i16_le(&rx_data[6]);
  g_supercap.rx_count++;
  g_supercap.last_rx_tick = HAL_GetTick();
}

static int16_t read_i16_le(const uint8_t *data)
{
  return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void UART1_SendTelemetry(void)
{
  char line[64];
  supercap_telemetry_t snap;

  __disable_irq();
  snap = g_supercap;
  __enable_irq();

  int len = snprintf(line, sizeof(line), "%d,%d,%d.%02d,%d\r\n",
                     (int)snap.input_power_w,
                     (int)snap.output_power_w,
                     (int)(snap.cap_voltage_x100 / 100),
                     (int)(snap.cap_voltage_x100 >= 0 ? snap.cap_voltage_x100 % 100 : -(snap.cap_voltage_x100 % 100)),
                     (int)snap.cap_actual_power_w);

  if (len > 0 && (size_t)len < sizeof(line))
  {
    (void)HAL_UART_Transmit(&huart6, (uint8_t *)line, (uint16_t)len, 4);
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)len, 4);
  }
}

static void DebugLed_Task(uint32_t now)
{
  static uint32_t last_tick = 0;
  if ((uint32_t)(now - last_tick) >= 500U)
  {
    last_tick = now;
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_11);
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
