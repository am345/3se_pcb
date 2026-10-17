#ifndef __USART_H
#define __USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;

void MX_USART1_UART_Init(void);
void MX_USART6_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif
