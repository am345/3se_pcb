#include "main.h"
#include "stdint.h"
#include "bsp_dwt.h"



static uint32_t CPU_FREQ_Hz;
static uint32_t CYCCNT_RountCount;
static float DWT_Timeline;

/**
 * @description: Initialize DWT (Data Watchpoint and Trace) peripheral for precise timing
 * @param CPU_Freq_mHz: System core clock frequency in MHz
 * @return: void
 * @note: Enables the hardware cycle counter
 */ 
void DWT_Init(uint32_t CPU_Freq_mHz)
{
    /* Enable DWT hardware trace */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset the DWT CYCCNT register */
    DWT->CYCCNT = (uint32_t)0u;

    /* Enable Cortex-M DWT CYCCNT register */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    CPU_FREQ_Hz = CPU_Freq_mHz * 1000000;
    CYCCNT_RountCount = 0;
    DWT_Timeline = 0;
}

/**
 * @description: Calculate the time difference (Delta T) in microseconds
 * @param cnt_last: Pointer to a variable storing the previous cycle count
 * @return: float representing elapsed time in microseconds (us)
 * @note: Typically called twice: once to log the start time, and once at the end to compute Delta T
 */ 
float DWT_GetDeltaT(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)(CPU_FREQ_Hz))*1e6f;
    *cnt_last = cnt_now;
    return dt;
}

/**
 * @description: Calculate the time difference (Delta T) in seconds using double precision
 * @param cnt_last: Pointer to a variable storing the previous cycle count
 * @return: double representing elapsed time in seconds (s)
 * @note: Typically called twice: once to log the start time, and once at the end to compute Delta T
 */ 
double DWT_GetDeltaT64(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;
    return dt;
}

/**
 * @description: Generate an absolute hardware timeline timestamp
 * @param {unused} 
 * @return: float Timeline timestamp in seconds (s)
 * @note: Must be called periodically to prevent overflow tracking miss
 */ 



float DWT_GetTimeline(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    static uint32_t CYCCNT_Last;

    if (cnt_now < CYCCNT_Last)
        CYCCNT_RountCount++;

    DWT_Timeline = ((uint64_t)CYCCNT_RountCount << 32) + cnt_now;
    DWT_Timeline /= (float)CPU_FREQ_Hz;

    CYCCNT_Last = cnt_now;
	
	return DWT_Timeline;
}
