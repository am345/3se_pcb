#include "tim.h"
#include "time_task.h"
#include "power_calculation.h"
#include "LED.h"
#include "hrtim.h"
#include "CAN_driver.h"
#include "vofa.h"
#include "math.h"  // Provide math functions like fabsf()

// Global timer counter used for 10ms tasks
uint8_t time10ms = 0;

// Hardware timer interrupt callback function
// Executed every time a specific timer completes a full period
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// Check if the interrupt was triggered by Timer 1
	if(htim == &htim1)
	{
		// Execute the core control calculation loop
		// This runs at 20 kHz (TIM1: 170MHz / 170 / 50 = 20kHz, period = 50us) to ensure fast PID response
		control_calc();
	}
	
	// Check if the interrupt was triggered by Timer 4
	if(htim == &htim4)
	{
		// Execute debugging and low-frequency tasks
		// This hook runs at a 1ms period (1 kHz)
		Debug_task();
	}
	
	// Check if the interrupt was triggered by Timer 6
	if(htim == &htim6)
	{
		// Handle the LED blinking task to indicate system heartbeat
		// If only powered by ST-Link (no main 24V bus power), skip blinking and keep LED solid green
		if (in.U_now < 10.0f) {
			// Do nothing here, preserving the solid green state set during main.c initialization
		} else {
			// Toggle the LED state to create a blinking effect
			if(LED_state == LED_OFF)
				LED_on();  // Turn on the LED if it was off
			else
				LED_off(); // Turn off the LED if it was on
		}
	}
}

// Background debug task running every 1 ms
void Debug_task()
{
	// Lightweight CAN timeout monitoring only. Any requested peripheral reset is
	// performed later by CAN_Service() in the main loop, outside this ISR.
	CAN_WatchdogCheck();

#if DCDC_OUTPUT == 2
	// Mode 2 Under-Voltage Lockout (UVLO) protection
	// Prevents the capacitor from backfeeding and keeping the MCU alive after main power is shut off
	static uint16_t uvlo_cnt = 0; // Counter to filter out temporary voltage dips
	
	// Only detect under-voltage when the system is fully initialized and running
	if(in.U_now < INV_MIN && init_flag == 1) 
	{
		uvlo_cnt++; // Increment the under-voltage duration counter
		
		// If the input voltage stays low continuously for more than 50 ms (50 * 1ms = 50ms)
		if(uvlo_cnt > 50) 
		{
			// Trigger a complete shutdown sequence to protect hardware
			DCDC_deinit();
			
			// Change the LED color to RED to visually indicate the fault state
			LED_SetColor(RED_COLOR);
			
			// Reset the initialization flag to prevent further operations
			init_flag = 0; 
		}
	}
	else
	{
		// Reset the counter immediately if the voltage recovers above the threshold
		uvlo_cnt = 0;
	}
#endif

	// Check if 10 milliseconds have passed (10 cycles of 1ms)
	if(time10ms == 9)
	{
		time10ms = 0; // Reset the cycle counter
		
		// Send power telemetry data over the CAN bus every 10ms (100 Hz frequency)
		power_send();
		VOFA_SendTelemetry();
	}
	else
	{
		// Increment the cycle counter for the next 1ms tick
		time10ms++;
	}
}
