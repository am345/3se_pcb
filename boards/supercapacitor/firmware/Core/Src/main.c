/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fdcan.h"
#include "hrtim.h"
#include "i2c.h"
#include "opamp.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_dwt.h"
#include "power_calculation.h"
#include "PID.h"
#include "OLED.h"
#include "LED.h"
#include "CAN_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define HRTIM_POWER_OUTPUTS (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

static void HRTIM_PowerOutputsOffFast(void)
{
  uint32_t duty_center;

  if (__HAL_RCC_HRTIM1_IS_CLK_ENABLED() == 0U)
  {
    return;
  }

  duty_center = HRTIM1->sMasterRegs.MPER / 2u;
  HRTIM1->sTimerxRegs[0].CMP1xR = duty_center;
  HRTIM1->sTimerxRegs[0].CMP3xR = duty_center;
  HRTIM1->sTimerxRegs[1].CMP1xR = duty_center;
  HRTIM1->sTimerxRegs[1].CMP3xR = duty_center;
  HRTIM1->sCommonRegs.ODISR = HRTIM_POWER_OUTPUTS;
  __DSB();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  // =============================================================
  // [CRITICAL SAFETY] Absolute first action: kill MOSFET gates!
  // During reset loops, HAL_Init + HAL_Delay(200) = 200ms+ of
  // floating HRTIM pins → half-bridge shoot-through → fire hazard.
  // Direct register writes, no HAL dependency.
  // =============================================================
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;  // Enable GPIO clocks
  __DSB();  // Ensure clock is ready before register access
  
  // PA8,PA9,PA10,PA11 → output push-pull, drive LOW
  GPIOA->MODER &= ~(0xFFu << 16);       // Clear mode bits for PA8-PA11
  GPIOA->MODER |=  (0x55u << 16);        // Set as output (01) for PA8-PA11
  GPIOA->BSRR   =  (0xFu << (8 + 16));  // Reset PA8-PA11 (write to upper 16 bits = reset)
  
  // PB14,PB15 → output push-pull, drive LOW
  GPIOB->MODER &= ~(0xFu << 28);         // Clear mode bits for PB14-PB15
  GPIOB->MODER |=  (0x5u << 28);          // Set as output (01) for PB14-PB15
  GPIOB->BSRR   =  (0x3u << 14) << 16;   // Reset PB14-PB15
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  
  // Power stability delay
  HAL_Delay(200);

  /* USER CODE BEGIN Init */
  // ---------------------------------------------------------
  // [SAFETY CRITICAL] 
  // Prevent MOS shoot-through due to floating/high-Z pins during power-up
  // Immediately pull down HRTIM related PWM pins
  // ---------------------------------------------------------
  __HAL_RCC_SYSCFG_CLK_ENABLE(); // Enable SYSCFG clock for OPAMP/ADC routing
  __HAL_RCC_GPIOA_CLK_ENABLE();  // Enable GPIOA clock
  __HAL_RCC_GPIOB_CLK_ENABLE();  // Enable GPIOB clock
  
  GPIO_InitTypeDef GPIO_InitStruct = {0}; // Define GPIO init struct
  
  // PA8, PA9, PA10, PA11 (Buck/Boost PWM Drive Pins)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET); // Output low initially
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11; // Select pins
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Push-pull output mode
  GPIO_InitStruct.Pull = GPIO_PULLDOWN; // Strong pull-down, ensuring low when undriven
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Low speed mode (initialization phase)
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); // Apply configuration
 
  // PB14, PB15 (Additional PWM Pins)
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET); // Output low initially
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15; // Select pins
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); // Apply configuration
  // ---------------------------------------------------------

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_HRTIM1_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_ADC5_Init();
  MX_FDCAN2_Init();
  MX_OPAMP3_Init();
  MX_OPAMP6_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_OPAMP1_Init();
  MX_OPAMP4_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(500); // Wait 500ms for power rails (1.25V Ref) to stabilize before ADC Calibration
	
    // LED Initialization & Indicate Red for Startup
	LED_init(); // Initialize onboard LED
	LED_SetColor(RED_COLOR); // Set LED to Red to indicate initialization phase
	
	// Initialize DWT hardware timing
	DWT_Init(170); // Initialize DWT cycle counter, base clock 170MHz, for precise delay calculation
	
	// Start ADC hardware calibration
	HAL_ADCEx_Calibration_Start(&hadc1,ADC_SINGLE_ENDED); // Calibrate ADC1 (Single-Ended Mode)
	HAL_ADCEx_Calibration_Start(&hadc2,ADC_SINGLE_ENDED); // Calibrate ADC2
	HAL_ADCEx_Calibration_Start(&hadc3,ADC_SINGLE_ENDED); // Calibrate ADC3
	HAL_ADCEx_Calibration_Start(&hadc4,ADC_SINGLE_ENDED); // Calibrate ADC4
	HAL_ADCEx_Calibration_Start(&hadc5,ADC_SINGLE_ENDED); // Calibrate ADC5
	
	// Internal OPAMP Initialization
	HAL_OPAMP_Start(&hopamp1); // Start OPAMP1
	HAL_OPAMP_Start(&hopamp3); // Start OPAMP3
	HAL_OPAMP_Start(&hopamp4); // Start OPAMP4
	HAL_OPAMP_Start(&hopamp6); // Start OPAMP6
	
	// Enable ADC & DMA Transfers
	HAL_Delay(5); // Small delay to let peripherals stabilize
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&in_v.valu, 1);
	HAL_ADC_Start_DMA(&hadc2, (uint32_t *)&cap_v.valu, 1);
	HAL_ADC_Start_DMA(&hadc3, (uint32_t *)&out_c.valu, 1); 	//Revert: ADC3 is Output Current
	HAL_ADC_Start_DMA(&hadc4, (uint32_t *)&in_c.valu, 1);   //Revert: ADC4 is Input Current
	HAL_ADC_Start_DMA(&hadc5, (uint32_t *)&cap_c.valu, 1);  // Start ADC5 DMA transfer -> Capacitor Current
	
	// OLED Initialization
	// TODO: Optimize OLED connection detection and support hot-plugging
	OLED_Init();

	// FDCAN Initialization
	if (FDCAN_FilterInit() != HAL_OK || FDCAN_start() != HAL_OK) {
		// Keep power control alive, but request a deferred full CAN recovery.
		can_health.needs_reinit = 1;
	}
	
	// Power Control Initialization
	DCDC_init(); // Initialize global dual-loop parameters and predefined HRTIM duty cycles
	
	// Start low-frequency debug timer and the 20kHz control ISR.
	// HRTIM runs PWM and ADC triggering independently after DCDC_init() starts its counters.
	HAL_TIM_Base_Start_IT(&htim4); // Start Timer 4 Interrupt (Debug Task Timebase)
	HAL_TIM_Base_Start_IT(&htim1); // Start Timer 1 Interrupt (20kHz Control Loop)
	
	// Post-Initialization Verification: Flash Green constant if main power exists but USB alone operates
	HAL_Delay(50); // Wait for ADC stabilizing reads
	if (in.U_now < 10.0f) {
		LED_SetColor(GREEN_COLOR); // No main power, just ST-Link debug power -> glow Green
	}

	// ====================== Hardware Watchdog (IWDG) ======================
	// Last-resort safety net: if control_calc() stops being called (HardFault,
	// clock failure, runaway code), IWDG forces a full MCU reset within ~100ms.
	// After reset, main() line 86-97 immediately kills MOSFET gates → safe.
	// Config: LSI≈32kHz, Prescaler=/32 → 1kHz tick, Reload=99 → 100ms timeout
	DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;  // Freeze IWDG during debugger halt (prevents reset on breakpoints)
	IWDG->KR  = 0xCCCC;  // Start IWDG (irreversible until reset)
	IWDG->KR  = 0x5555;  // Enable write access to PR/RLR
	IWDG->PR  = 3;       // Prescaler /32
	IWDG->RLR = 99;      // Reload 99 → (99+1)/1000 = 100ms timeout
	while (IWDG->SR);    // Wait for register update complete
	IWDG->KR  = 0xAAAA;  // First feed

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		// Execute deferred CAN recovery outside interrupt context.
		CAN_Service();

		memset(OLED_GRAM, 0, sizeof(OLED_GRAM));

		// Line 1~3: Live telemetry of Input/Output/Capacitor data (Common across modes)
		OLED_printf(0, 0, "IN %4.1f;%5.2f;%3.0f", in.U_now, in.I_now, in.P_now);
		OLED_printf(0, 12, "OT %4.1f;%5.2f;%3.0f", out.U_now, out.I_now, out.P_now);
		OLED_printf(0, 24, "CP %4.1f;%5.2f;%3.0f", cap.U_now, cap.I_now, cap.P_now);

		#if OLED_MODE == 0
		// === Debug Mode: Display Raw ADC Register Values ===
		OLED_printf(0, 36, "V:i%4u c%4u", in_v.valu, cap_v.valu);
		OLED_printf(0, 48, "C:i%4u c%4u o%4u", in_c.valu, cap_c.valu, out_c.valu);
		#else
		// === Production/Vehicle Mode: Display live CAN health ===
		const char *rx_state = CAN_RxRecent() ? "OK" : "--";
		const char *tx_state = CAN_TxRecent() ? "OK" : ((can_health.tx_fail_count || can_health.tx_error_active || can_health.needs_reinit) ? "FL" : "--");
		OLED_printf(0, 36, "B%d RX:%s TX:%s", BOARD_NUM, rx_state, tx_state);
		OLED_printf(0, 48, "R%04lu T%04lu E%02lu",
		            (unsigned long)(can_health.rx_count % 10000u),
		            (unsigned long)(can_health.tx_count % 10000u),
		            (unsigned long)(can_health.error_count % 100u));
		#endif

		OLED_Refresh_Gram();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  HRTIM_PowerOutputsOffFast();
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
