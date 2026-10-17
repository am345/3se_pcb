#ifndef __filter_H
#define	__filter_H

// ====================== First-Order Low-Pass Filter ======================
typedef struct
{
	float Input;  // Current input value
	float Output; // Current output/filtered value
	float Fc;	  // Cutoff frequency (Hz)
	float Fs;	  // Sampling frequency (Hz)
	float Ka;	  // Feedforward filter coefficient
	float Kb;	  // Feedback filter coefficient
} LOWPASS_FILTER_STRUCT;

#define PI2   6.28318530F

extern LOWPASS_FILTER_STRUCT lpf[5];

// ====================== Function Declarations ======================
extern void low_filter_init(LOWPASS_FILTER_STRUCT *p, float sample_f, float cutoff_f);
extern void low_filter_calc(LOWPASS_FILTER_STRUCT *p);

#endif
