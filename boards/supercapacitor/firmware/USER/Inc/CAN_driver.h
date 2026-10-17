#ifndef _CAN_DRIVER_H
#define _CAN_DRIVER_H

#include "fdcan.h"
#include "power_calculation.h"	// Include BOARD_NUM, ROBOT_TYPE

// ====================== CAN ID Configuration ======================
#define SEND_ID			0x211		// TX: State feedback to chassis

// RX ID: Determined by ROBOT_TYPE at compile time
#if ROBOT_TYPE == 1
  #define RECEIVE_ID	0x200		// Infantry / Sentry
#elif ROBOT_TYPE == 2
  #define RECEIVE_ID	0x2FF		// Hero
#else
  #error "Undefined ROBOT_TYPE, cannot determine CAN RECEIVE_ID"
#endif

#define RECEIVE_NUM		2			// Byte-pair index for power limit (extracts Byte[4:5])

// ====================== Watchdog Configuration ======================
#define CAN_WDG_TIMEOUT_MS		500		// No RX for 500ms after first contact -> reinit
#define CAN_TX_FAIL_REINIT_THR	50		// Consecutive TX failures before reinit

// ====================== CAN Health Monitoring ======================
typedef struct
{
	uint32_t rx_count;				// Total successfully received frames
	uint32_t tx_count;				// Total ACK-confirmed transmitted frames
	uint32_t tx_enqueue_count;		// Total frames accepted into Tx FIFO
	uint32_t tx_fail_count;			// Consecutive failed TX attempts
	uint32_t error_count;			// Total error events
	uint32_t busoff_count;			// Bus-Off events (critical)
	uint32_t fifo_overflow_count;	// RX FIFO overflow / message lost events
	uint32_t reinit_count;			// Total FDCAN reinitializations performed
	uint8_t  bus_active;			// 1 = at least one frame received since boot
	uint8_t  needs_reinit;			// Flag: set in ISR, cleared after reinit in task context
	uint8_t  tx_error_active;		// 1 = TX/protocol error seen since last ACK-confirmed frame
} CAN_Health_t;

extern volatile CAN_Health_t can_health;

// ====================== Public Interface ======================
HAL_StatusTypeDef FDCAN_FilterInit(void);
HAL_StatusTypeDef FDCAN_start(void);
void FCAN_transmit(FDCAN_HandleTypeDef *hfdcan, uint32_t can_id, uint32_t DataLength, uint8_t tx_data[]);
void power_send(void);
void CAN_WatchdogCheck(void);		// Lightweight 1ms ISR-side timeout check only
void CAN_Service(void);			// Call from main context for deferred FDCAN recovery
uint8_t CAN_RxRecent(void);
uint8_t CAN_TxRecent(void);

#endif
