#include "usart.h"

#define USART2_DEBUG_BAUD 460800u

static volatile const uint8_t *usart2_tx_ptr = 0;
static volatile uint16_t usart2_tx_left = 0;
static volatile uint8_t usart2_tx_busy = 0;

void MX_USART2_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint32_t pclk;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  USART2->CR1 = 0u;
  USART2->CR2 = 0u;
  USART2->CR3 = 0u;

  pclk = HAL_RCC_GetPCLK1Freq();
  USART2->BRR = (pclk + (USART2_DEBUG_BAUD / 2u)) / USART2_DEBUG_BAUD;
  USART2->ICR = USART_ICR_TCCF;
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;

  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

uint8_t USART2_TryStartTx(const uint8_t *data, uint16_t len)
{
  if (data == 0 || len == 0u || usart2_tx_busy != 0u) {
    return 0u;
  }

  usart2_tx_ptr = data;
  usart2_tx_left = len;
  usart2_tx_busy = 1u;
  USART2->CR1 |= USART_CR1_TXEIE_TXFNFIE;

  return 1u;
}

void USART2_TxIrqHandler(void)
{
  if ((USART2->CR1 & USART_CR1_TXEIE_TXFNFIE) &&
      (USART2->ISR & USART_ISR_TXE_TXFNF)) {
    if (usart2_tx_left > 0u) {
      USART2->TDR = *usart2_tx_ptr++;
      usart2_tx_left--;
    }

    if (usart2_tx_left == 0u) {
      USART2->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
      USART2->CR1 |= USART_CR1_TCIE;
    }
  }

  if ((USART2->CR1 & USART_CR1_TCIE) && (USART2->ISR & USART_ISR_TC)) {
    USART2->ICR = USART_ICR_TCCF;
    USART2->CR1 &= ~USART_CR1_TCIE;
    usart2_tx_busy = 0u;
  }
}
