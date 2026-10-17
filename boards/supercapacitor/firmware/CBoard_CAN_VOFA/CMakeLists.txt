cmake_minimum_required(VERSION 3.22)

project(CBoard_CAN_VOFA C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(TARGET_NAME CBoard_CAN_VOFA)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Core/Linker/STM32F407IGHx_FLASH.ld)

add_executable(${TARGET_NAME}.elf
  Core/Startup/startup_stm32f407xx.s
  Src/main.c
  Src/can.c
  Src/gpio.c
  Src/usart.c
  Src/stm32f4xx_hal_msp.c
  Src/stm32f4xx_it.c
  Src/system_stm32f4xx.c
  Src/syscalls.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

target_compile_definitions(${TARGET_NAME}.elf PRIVATE
  USE_HAL_DRIVER
  STM32F407xx
)

target_include_directories(${TARGET_NAME}.elf PRIVATE
  Inc
  Drivers/STM32F4xx_HAL_Driver/Inc
  Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
  Drivers/CMSIS/Device/ST/STM32F4xx/Include
  Drivers/CMSIS/Include
)

target_compile_options(${TARGET_NAME}.elf PRIVATE
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -Wall
  -Wextra
  -Wno-unused-parameter
  -ffunction-sections
  -fdata-sections
)

target_link_options(${TARGET_NAME}.elf PRIVATE
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -T${LINKER_SCRIPT}
  -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map
  -Wl,--gc-sections
  --specs=nano.specs
  --specs=nosys.specs
)

add_custom_command(TARGET ${TARGET_NAME}.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:${TARGET_NAME}.elf>
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE_NAME:${TARGET_NAME}.elf> ${TARGET_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${TARGET_NAME}.elf> ${TARGET_NAME}.bin
  VERBATIM
)
