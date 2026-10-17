#ifndef PID_H
#define PID_H

enum PID_MODE
{
    PID_POSITION = 0, // Positional PID Mode
    PID_DELTA         // Incremental / Delta PID Mode
};

/**
  * @brief          PID struct data definition
  * @param          mode: PID_POSITION (Normal PID) or PID_DELTA (Incremental PID)
  * @param          Kp: Proportional gain
  * @param          Ki: Integral gain
  * @param          Kd: Derivative gain
  * @param          max_out: Maximum absolute output
  * @param          min_out: Minimum absolute output
  * @param          max_iout: Maximum integral output
  * @param          min_iout: Minimum integral output
  */
typedef struct
{
    uint8_t mode; // Operating mode
    
    // Core PID parameters
    float Kp;
    float Ki;
    float Kd;
    float T; // Calculation period / Sampling time
    
    // Bounds and Limits
    float max_out;  // Maximum output clamp
    float min_out;  // Minimum output clamp
    float max_iout; // Maximum integral term clamp
    float min_iout; // Minimum integral term clamp
    
    // Signals
    float set; // Setpoint / Target value
    float fdb; // Feedback / Actual current value

    // Calculated terms and output
    float out;  // Final controller output
    float Pout; // Proportional output
    float Iout; // Integral output
    float Dout; // Derivative output
    
    // History buffers
    float Dbuf[3];  // Derivative term history: [0] Current, [1] Previous, [2] Oldest
    float error[3]; // Error term history: [0] Current, [1] Previous, [2] Oldest

} pid_type_def;

void PID_init(pid_type_def *pid, uint8_t mode, const float p,const float i,const float d, float max_out, float max_iout,float min_out,float min_iout);
float PID_calc(pid_type_def *pid, float ref, float set);
void PID_clear(pid_type_def *pid);

extern pid_type_def curr_loop;
extern pid_type_def volt_high_loop;
#endif
