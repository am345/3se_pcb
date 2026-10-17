cmake_minimum_required(VERSION 3.22)

project(SuperCap_V3_1 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TARGET_NAME SuperCap_V3_1)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Core/Linker/STM32G474CETx_FLASH.ld)

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(PROJECT_DEFINES
    USE_HAL_DRIVER
    STM32G474xx
)

set(PROJECT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USER/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)

set(PROJECT_SOURCES
    Core/Startup/startup_stm32g474xx.s
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/adc.c
    Core/Src/dma.c
    Core/Src/fdcan.c
    Core/Src/hrtim.c
    Core/Src/i2c.c
    Core/Src/opamp.c
    Core/Src/tim.c
    Core/Src/usart.c
    Core/Src/stm32g4xx_it.c
    Core/Src/stm32g4xx_hal_msp.c
    Core/Src/system_stm32g4xx.c
    Core/Src/syscalls.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_ll_adc.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_fdcan.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ramfunc.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_hrtim.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_opamp.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_opamp_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c
    USER/Src/bsp_dwt.c
    USER/Src/CAN_driver.c
    USER/Src/filter.c
    USER/Src/LED.c
    USER/Src/OLED.c
    USER/Src/PID.c
    USER/Src/power_calculation.c
    USER/Src/time_task.c
    USER/Src/vofa.c
)

add_executable(${TARGET_NAME}.elf ${PROJECT_SOURCES})

target_compile_definitions(${TARGET_NAME}.elf PRIVATE ${PROJECT_DEFINES})
target_include_directories(${TARGET_NAME}.elf PRIVATE ${PROJECT_INCLUDES})

target_compile_options(${TARGET_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -ffunction-sections
    -fdata-sections
    -fno-common
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -g0>
)

target_link_options(${TARGET_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -u _printf_float
)

target_link_libraries(${TARGET_NAME}.elf PRIVATE m c nosys)

add_custom_command(TARGET ${TARGET_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:${TARGET_NAME}.elf>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE_NAME:${TARGET_NAME}.elf> ${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${TARGET_NAME}.elf> ${TARGET_NAME}.bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
)
