#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void MX_USART2_UART_Init(void);
uint8_t USART2_TryStartTx(const uint8_t *data, uint16_t len);
void USART2_TxIrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif
