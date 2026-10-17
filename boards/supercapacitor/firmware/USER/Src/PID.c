#include "main.h"
#include "PID.h"

// Macro to clamp an input value strictly below a specified maximum limit
#define LimitMax(input, max) \
  do {                       \
    if (input > max)         \
    {                        \
      input = max;           \
    }                        \
  } while(0)
	
// Macro to clamp an input value strictly above a specified minimum limit
#define LimitMin(input, min) \
  do {                       \
    if (input < min)         \
    {                        \
      input = min;           \
    }                        \
  } while(0)

// Global PID controller instances
pid_type_def curr_loop;      // Core current regulation control loop
pid_type_def volt_high_loop; // 32V high-voltage CV ceiling controller

/**
  * @brief          Initialize the PID controller struct data
  * @param[out]     pid: Pointer to the PID structure to initialize
  * @param[in]      mode: Operating mode (PID_POSITION for positional, PID_DELTA for incremental)
  * @param[in]      p: Proportional gain (Kp)
  * @param[in]      i: Integral gain (Ki)
  * @param[in]      d: Derivative gain (Kd)
  * @param[in]      max_out: Maximum absolute output limit
  * @param[in]      max_iout: Maximum absolute integral output limit
  * @param[in]      min_out: Minimum absolute output limit
  * @param[in]      min_iout: Minimum absolute integral output limit
  * @retval         none
  */
void PID_init(pid_type_def *pid, uint8_t mode, const float p, const float i, const float d, float max_out, float max_iout, float min_out, float min_iout)
{
  if (pid == NULL)
  {
    return; // Safety guard against null pointer
  }
  
  // Assign configuration parameters
  pid->mode = mode;
  pid->Kp = p;
  pid->Ki = i;
  pid->Kd = d;
  pid->max_out = max_out;
  pid->max_iout = max_iout;
  pid->min_out = min_out;
  pid->min_iout = min_iout;  
  
  // Clear all running state history buffers
  pid->Dbuf[0] = pid->Dbuf[1] = pid->Dbuf[2] = 0.0f;
  pid->error[0] = pid->error[1] = pid->error[2] = pid->Pout = pid->Iout = pid->Dout = pid->out = 0.0f;
  pid->fdb = 0.0f;
  pid->set = 0.0f;
}

/**
  * @brief          Calculate the next iteration for the PID controller
  * @param[out]     pid: Pointer to the PID structure
  * @param[in]      ref: Feedback measurement (current actual state)
  * @param[in]      set: Target setpoint
  * @retval         PID controller output value
  */
float PID_calc(pid_type_def *pid, float ref, float set)
{
  if (pid == NULL)
  {
    return 0.0f; // Safety guard
  }

  // Update history buffers (First-In-First-Out shifting)
  pid->error[2] = pid->error[1];
  pid->error[1] = pid->error[0];
  
  // Record current setpoint and feedback for telemetry
  pid->set = set; 
  pid->fdb = ref; 
  
  // Calculate the current cycle's error
  pid->error[0] = set - ref; 
  
  if (pid->mode == PID_POSITION) // Positional PID Mode
  {
    pid->Pout = pid->Kp * pid->error[0]; // Proportional component
    pid->Iout += pid->Ki * pid->error[0]; // Integral component (Cumulative)
    
    // Update derivative term history buffers
    pid->Dbuf[2] = pid->Dbuf[1];
    pid->Dbuf[1] = pid->Dbuf[0];
    pid->Dbuf[0] = (pid->error[0] - pid->error[1]); // Calculate difference
    pid->Dout = pid->Kd * pid->Dbuf[0]; // Derivative component
    
    // Clamp the integral term to prevent integral windup
    LimitMax(pid->Iout, pid->max_iout); 
    LimitMin(pid->Iout, pid->min_iout); 
    
    // Sum components for final control output
    pid->out = pid->Pout + pid->Iout + pid->Dout; 
    
    // Clamp the overall output strictly within safe bounds
    LimitMax(pid->out, pid->max_out); 
    LimitMin(pid->out, pid->min_out); 
  }
  else if (pid->mode == PID_DELTA) // Incremental (Delta) PID Mode
  {
    // Proportional increment (Kp * Delta Error)
    pid->Pout = pid->Kp * (pid->error[0] - pid->error[1]); 
    // Integral increment (Ki * Current Error, no accumulation needed here)
    pid->Iout = pid->Ki * pid->error[0]; 
    
    // Update derivative term history buffers
    pid->Dbuf[2] = pid->Dbuf[1];
    pid->Dbuf[1] = pid->Dbuf[0];
    // Calculate second-order difference for derivative increment
    pid->Dbuf[0] = (pid->error[0] - 2.0f * pid->error[1] + pid->error[2]); 
    pid->Dout = pid->Kd * pid->Dbuf[0]; 
    
    // Integrate the calculated increment onto the ongoing output
    pid->out += pid->Pout + pid->Iout + pid->Dout; 
    
    // Clamp the newly accumulated output
    LimitMax(pid->out, pid->max_out); 
    LimitMin(pid->out, pid->min_out); 
  }
  
  // Return the new control parameter
  return pid->out; 
}

/**
  * @brief          Clear all states and history of a PID controller
  * @param[out]     pid: Pointer to the PID structure to reset
  * @retval         none
  */
void PID_clear(pid_type_def *pid)
{
  if (pid == NULL)
  {
    return; // Safety guard
  }

  // Erase all running state arrays and accumulators
  pid->error[0] = pid->error[1] = pid->error[2] = 0.0f;
  pid->Dbuf[0] = pid->Dbuf[1] = pid->Dbuf[2] = 0.0f;
  pid->out = pid->Pout = pid->Iout = pid->Dout = 0.0f;
  pid->fdb = pid->set = 0.0f;
}

