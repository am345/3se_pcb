#ifndef _BSP_DWT_H
#define _BSP_DWT_H

void DWT_Init(uint32_t CPU_Freq_mHz);
float DWT_GetDeltaT(uint32_t *cnt_last);
double DWT_GetDeltaT64(uint32_t *cnt_last);
float DWT_GetTimeline(void);

#endif /* BSP_DWT_H_ */
