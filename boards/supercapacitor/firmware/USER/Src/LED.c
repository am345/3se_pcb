#include "tim.h"
#include "stdint.h"
#include "LED.h"

uint8_t LED_state;

/**
 * @description: LED initialization, enabling PWM clocks and channels
 * @param {unused} 
 * @return: void
 * @note: 
 */ 
void LED_init()
 {
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); 	// Blue Channel
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); 	// Red Channel
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); 	// Green Channel
	TIM3->CCR2 = 0; // Initialize Red PWM duty to 0
	TIM3->CCR3 = 0; // Initialize Green PWM duty to 0
	TIM3->CCR1 = 0; // Initialize Blue PWM duty to 0
	LED_state = LED_OFF;
 }
 
 /**
  * @description: Set specific RGB values for the LED
  * @param red: Red brightness (0-255 mapped to PWM)
  * @param green: Green brightness (0-255 mapped to PWM)
  * @param blue: Blue brightness (0-255 mapped to PWM)
  * @return: void
  * @note: 
  */ 
void LED_SetColor(uint8_t red, uint8_t green, uint8_t blue)
 {
	TIM3->CCR2 = red;   // Update Red PWM duty
	TIM3->CCR3 = green; // Update Green PWM duty
	TIM3->CCR1 = blue;  // Update Blue PWM duty
 }
 
/**
 * @description: Turn on the LED by starting PWM outputs
 * @param {unused} 
 * @return: void
 * @note: 
 */ 
 void LED_on()
 {
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); 	// Blue Channel
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); 	// Red Channel
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); 	// Green Channel
	LED_state = LED_ON;
 }
 
 /**
 * @description: Turn off the LED by stopping PWM outputs
 * @param {unused} 
 * @return: void
 * @note: 
 */ 
 void LED_off()
 {
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1); 	// Blue Channel
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2); 	// Red Channel
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3); 	// Green Channel
	LED_state = LED_OFF;
 }
 
 /**
  * @description: Blink the LED at a fixed frequency using Timer 6
  * @param frequency: Blinking frequency (Hz) or 0 to stop blinking
  * @return: void
  * @note: Relies on TIM6 interrupt flipping state
  */ 
 void LED_blink(uint8_t frequency)
 {
	// TIM6 Prescaler is 17000-1, meaning base counting frequency is 10kHz
	 if(frequency == 0)
		 HAL_TIM_Base_Stop_IT(&htim6); // Stop timer to completely halt blinking
	 else
	 {
        // Calculate Auto-Reload Register value based on target frequency
        // Multiplication by 2 is used because one full cycle requires an ON and OFF phase
		TIM6->ARR = 10000/2/frequency - 1;
		HAL_TIM_Base_Start_IT(&htim6); // Start timer with interrupt enable
	 }
 }
