#include "filter.h"

LOWPASS_FILTER_STRUCT lpf[5];

// ====================== First-Order Low-Pass Filter ======================
// Initialize the low-pass filter with a specified sampling frequency and cutoff frequency
void low_filter_init(LOWPASS_FILTER_STRUCT *p, float sample_f, float cutoff_f)
{
  float Tc;
  p->Fc = cutoff_f; // Set the cutoff frequency (Hz)
  p->Fs = sample_f; // Set the sampling frequency (Hz)
  
  // If frequencies are invalid, bypass the filter completely
  if (p->Fc <= 0.0f || p->Fs <= 0.0f)
  {
    p->Ka = 1.0f; // Pass input directly to output
    p->Kb = 0.0f; // Ignore previous output values
    return;
  }

  // Calculate the filter coefficients based on the RC time constant equivalent
  Tc = 1.0f / (PI2 * p->Fc);
  p->Ka = 1.0f / (1.0f + Tc * p->Fs);
  p->Kb = 1.0f - p->Ka;
  
  // Clear the history buffers
  p->Input = 0.0f;
  p->Output = 0.0f;
}

// Calculate the next filter output using the first-order recursive equation
void low_filter_calc(LOWPASS_FILTER_STRUCT *p)
{
  // Output = alpha * Input + (1 - alpha) * Previous_Output
  p->Output = p->Ka * p->Input + p->Kb * p->Output;
}
