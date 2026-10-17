.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global g_pfnVectors
.global Reset_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_estack
  mov sp, r0

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit
  b LoopFillZerobss
CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4
  b LoopCopyDataInit

LoopFillZerobss:
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
FillZerobss:
  cmp r0, r1
  bcc FillZerobssLoop
  b CallSystemInit
FillZerobssLoop:
  str r2, [r0]
  adds r0, r0, #4
  b FillZerobss

CallSystemInit:
  bl SystemInit
  bl __libc_init_array
  bl main
LoopForever:
  b LoopForever

.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
.size Default_Handler, .-Default_Handler

.macro def_irq_handler handler
  .weak \handler
  .set \handler, Default_Handler
.endm

def_irq_handler NMI_Handler
def_irq_handler HardFault_Handler
def_irq_handler MemManage_Handler
def_irq_handler BusFault_Handler
def_irq_handler UsageFault_Handler
def_irq_handler SVC_Handler
def_irq_handler DebugMon_Handler
def_irq_handler PendSV_Handler
def_irq_handler SysTick_Handler
def_irq_handler WWDG_IRQHandler
def_irq_handler PVD_PVM_IRQHandler
def_irq_handler RTC_TAMP_LSECSS_IRQHandler
def_irq_handler RTC_WKUP_IRQHandler
def_irq_handler FLASH_IRQHandler
def_irq_handler RCC_IRQHandler
def_irq_handler EXTI0_IRQHandler
def_irq_handler EXTI1_IRQHandler
def_irq_handler EXTI2_IRQHandler
def_irq_handler EXTI3_IRQHandler
def_irq_handler EXTI4_IRQHandler
def_irq_handler DMA1_Channel1_IRQHandler
def_irq_handler DMA1_Channel2_IRQHandler
def_irq_handler DMA1_Channel3_IRQHandler
def_irq_handler DMA1_Channel4_IRQHandler
def_irq_handler DMA1_Channel5_IRQHandler
def_irq_handler DMA1_Channel6_IRQHandler
def_irq_handler DMA1_Channel7_IRQHandler
def_irq_handler ADC1_2_IRQHandler
def_irq_handler USB_HP_IRQHandler
def_irq_handler USB_LP_IRQHandler
def_irq_handler FDCAN1_IT0_IRQHandler
def_irq_handler FDCAN1_IT1_IRQHandler
def_irq_handler EXTI9_5_IRQHandler
def_irq_handler TIM1_BRK_TIM15_IRQHandler
def_irq_handler TIM1_UP_TIM16_IRQHandler
def_irq_handler TIM1_TRG_COM_TIM17_IRQHandler
def_irq_handler TIM1_CC_IRQHandler
def_irq_handler TIM2_IRQHandler
def_irq_handler TIM3_IRQHandler
def_irq_handler TIM4_IRQHandler
def_irq_handler I2C1_EV_IRQHandler
def_irq_handler I2C1_ER_IRQHandler
def_irq_handler I2C2_EV_IRQHandler
def_irq_handler I2C2_ER_IRQHandler
def_irq_handler SPI1_IRQHandler
def_irq_handler SPI2_IRQHandler
def_irq_handler USART1_IRQHandler
def_irq_handler USART2_IRQHandler
def_irq_handler USART3_IRQHandler
def_irq_handler EXTI15_10_IRQHandler
def_irq_handler RTC_Alarm_IRQHandler
def_irq_handler USBWakeUp_IRQHandler
def_irq_handler TIM8_BRK_IRQHandler
def_irq_handler TIM8_UP_IRQHandler
def_irq_handler TIM8_TRG_COM_IRQHandler
def_irq_handler TIM8_CC_IRQHandler
def_irq_handler ADC3_IRQHandler
def_irq_handler FMC_IRQHandler
def_irq_handler LPTIM1_IRQHandler
def_irq_handler TIM5_IRQHandler
def_irq_handler SPI3_IRQHandler
def_irq_handler UART4_IRQHandler
def_irq_handler UART5_IRQHandler
def_irq_handler TIM6_DAC_IRQHandler
def_irq_handler TIM7_DAC_IRQHandler
def_irq_handler DMA2_Channel1_IRQHandler
def_irq_handler DMA2_Channel2_IRQHandler
def_irq_handler DMA2_Channel3_IRQHandler
def_irq_handler DMA2_Channel4_IRQHandler
def_irq_handler DMA2_Channel5_IRQHandler
def_irq_handler ADC4_IRQHandler
def_irq_handler ADC5_IRQHandler
def_irq_handler UCPD1_IRQHandler
def_irq_handler COMP1_2_3_IRQHandler
def_irq_handler COMP4_5_6_IRQHandler
def_irq_handler COMP7_IRQHandler
def_irq_handler HRTIM1_Master_IRQHandler
def_irq_handler HRTIM1_TIMA_IRQHandler
def_irq_handler HRTIM1_TIMB_IRQHandler
def_irq_handler HRTIM1_TIMC_IRQHandler
def_irq_handler HRTIM1_TIMD_IRQHandler
def_irq_handler HRTIM1_TIME_IRQHandler
def_irq_handler HRTIM1_FLT_IRQHandler
def_irq_handler HRTIM1_TIMF_IRQHandler
def_irq_handler CRS_IRQHandler
def_irq_handler SAI1_IRQHandler
def_irq_handler TIM20_BRK_IRQHandler
def_irq_handler TIM20_UP_IRQHandler
def_irq_handler TIM20_TRG_COM_IRQHandler
def_irq_handler TIM20_CC_IRQHandler
def_irq_handler FPU_IRQHandler
def_irq_handler I2C4_EV_IRQHandler
def_irq_handler I2C4_ER_IRQHandler
def_irq_handler SPI4_IRQHandler
def_irq_handler FDCAN2_IT0_IRQHandler
def_irq_handler FDCAN2_IT1_IRQHandler
def_irq_handler FDCAN3_IT0_IRQHandler
def_irq_handler FDCAN3_IT1_IRQHandler
def_irq_handler RNG_IRQHandler
def_irq_handler LPUART1_IRQHandler
def_irq_handler I2C3_EV_IRQHandler
def_irq_handler I2C3_ER_IRQHandler
def_irq_handler DMAMUX_OVR_IRQHandler
def_irq_handler QUADSPI_IRQHandler
def_irq_handler DMA1_Channel8_IRQHandler
def_irq_handler DMA2_Channel6_IRQHandler
def_irq_handler DMA2_Channel7_IRQHandler
def_irq_handler DMA2_Channel8_IRQHandler
def_irq_handler CORDIC_IRQHandler
def_irq_handler FMAC_IRQHandler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .word WWDG_IRQHandler
  .word PVD_PVM_IRQHandler
  .word RTC_TAMP_LSECSS_IRQHandler
  .word RTC_WKUP_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_IRQHandler
  .word DMA1_Channel3_IRQHandler
  .word DMA1_Channel4_IRQHandler
  .word DMA1_Channel5_IRQHandler
  .word DMA1_Channel6_IRQHandler
  .word DMA1_Channel7_IRQHandler
  .word ADC1_2_IRQHandler
  .word USB_HP_IRQHandler
  .word USB_LP_IRQHandler
  .word FDCAN1_IT0_IRQHandler
  .word FDCAN1_IT1_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_TIM15_IRQHandler
  .word TIM1_UP_TIM16_IRQHandler
  .word TIM1_TRG_COM_TIM17_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word USBWakeUp_IRQHandler
  .word TIM8_BRK_IRQHandler
  .word TIM8_UP_IRQHandler
  .word TIM8_TRG_COM_IRQHandler
  .word TIM8_CC_IRQHandler
  .word ADC3_IRQHandler
  .word FMC_IRQHandler
  .word LPTIM1_IRQHandler
  .word TIM5_IRQHandler
  .word SPI3_IRQHandler
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word TIM6_DAC_IRQHandler
  .word TIM7_DAC_IRQHandler
  .word DMA2_Channel1_IRQHandler
  .word DMA2_Channel2_IRQHandler
  .word DMA2_Channel3_IRQHandler
  .word DMA2_Channel4_IRQHandler
  .word DMA2_Channel5_IRQHandler
  .word ADC4_IRQHandler
  .word ADC5_IRQHandler
  .word UCPD1_IRQHandler
  .word COMP1_2_3_IRQHandler
  .word COMP4_5_6_IRQHandler
  .word COMP7_IRQHandler
  .word HRTIM1_Master_IRQHandler
  .word HRTIM1_TIMA_IRQHandler
  .word HRTIM1_TIMB_IRQHandler
  .word HRTIM1_TIMC_IRQHandler
  .word HRTIM1_TIMD_IRQHandler
  .word HRTIM1_TIME_IRQHandler
  .word HRTIM1_FLT_IRQHandler
  .word HRTIM1_TIMF_IRQHandler
  .word CRS_IRQHandler
  .word SAI1_IRQHandler
  .word TIM20_BRK_IRQHandler
  .word TIM20_UP_IRQHandler
  .word TIM20_TRG_COM_IRQHandler
  .word TIM20_CC_IRQHandler
  .word FPU_IRQHandler
  .word I2C4_EV_IRQHandler
  .word I2C4_ER_IRQHandler
  .word SPI4_IRQHandler
  .word 0
  .word FDCAN2_IT0_IRQHandler
  .word FDCAN2_IT1_IRQHandler
  .word FDCAN3_IT0_IRQHandler
  .word FDCAN3_IT1_IRQHandler
  .word RNG_IRQHandler
  .word LPUART1_IRQHandler
  .word I2C3_EV_IRQHandler
  .word I2C3_ER_IRQHandler
  .word DMAMUX_OVR_IRQHandler
  .word QUADSPI_IRQHandler
  .word DMA1_Channel8_IRQHandler
  .word DMA2_Channel6_IRQHandler
  .word DMA2_Channel7_IRQHandler
  .word DMA2_Channel8_IRQHandler
  .word CORDIC_IRQHandler
  .word FMAC_IRQHandler
.size g_pfnVectors, .-g_pfnVectors
