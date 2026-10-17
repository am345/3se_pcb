#ifndef _POWER_CALCULATION_H
#define _POWER_CALCULATION_H

#include "stdint.h"

// ====================== Function Switches ======================

#define LOW_PASS_FILTTER	1		// 0: Disable LPF  1: Enable LPF
#define DCDC_OUTPUT			3		// 1: Buck half-bridge test  2: Boost half-bridge test  3: PID closed-loop (normal operation)
#define OLED_MODE			1	//   0: Debug mode (shows raw ADC)  1: Production mode (shows Board ID + CAN)
#define BOARD_NUM			2// Calibration profile: 0=theory, 1~4=calibrated boards; 5~6=pending calibration
#define ROBOT_TYPE			1// Robot type: 1=Infantry/Sentry(0x200), 2=Hero(0x2FF)

// ====================== PWM Parameters ======================
#define MAX_PWM_RATIO 0.97f			// Maximum control duty cycle (clamped to 97% to reserve 3% for bootstrap capacitor charging)
#define MIN_PWM_RATIO 0.03f			// Minimum control duty cycle (symmetric clamp at 3%)

// ====================== ADC Parameters ======================
#define adc_maxvalu 4095
#define adc_vref 3.2958f
#define adc_conv_fact adc_vref / adc_maxvalu
#define V_GAIN 20.0f		// Differential amplifier gain (OPA2350, Rfb=1k/Rin=20k, gain=1/20)
#define I_GAIN 20.0f		// INA240A1 amplifier gain = 20V/V
#define R_SAMP_INOUT 0.003f	// in_c/out_c sense resistor = 3mΩ
#define R_SAMP_CAP 0.0015f	// cap_c sense resistor = 1.5mΩ (2×3mΩ parallel) on all boards
#define I_BIAS 776			// INA240 zero-point ADC bias (ADC ≈ 776)

// ====================== Hardware Safety Limits ======================
#define INV_MIN 19.0f			// Minimum input voltage (UVLO: shutdown <19V); unverified setting, requires hardware tuning.
#define INV_RECOVERY_HYSTERESIS_V 4.0f	// Input must recover above INV_MIN + this margin before the power-loss latch can clear.
#define max_volt_ratio 1.7f		// Maximum voltage ratio (1.7x boost → max 40.8V. Original MOSFET BSC070N10NS3G=100V OK; new CSD18540Q5BT=60V, margin is tight!)
#define CAPV_MAX 28.0f			// Normal target capacitor voltage / charge upper limit
#define CAPV_MIN 7.0f			// Supercapacitor discharge voltage lower limit (discharge strictly forbidden below 7V)
#define CAPV_DERATE_START 15.0f	// Start smoothly derating discharge current below this voltage.
#define max_capc_nega -14.5f	// Maximum discharge current limit; unverified aggressive setting, requires hardware tuning.
#define max_capc_posi 5.0f		// Normal maximum charge current limit; unverified setting, requires hardware tuning.
#define max_capc_posi_regen 10.0f	// Braking regen maximum cap-charge current.
#define ESR_COMP_OHM 0.35f		// Estimated ESR compensation; unverified setting, requires hardware tuning.

// ====================== Feed-forward and Cascaded Control Parameters ======================
#define CONTROL_LOOP_HZ 20000.0f		// Control loop frequency; unverified accelerated setting, requires hardware timing/current-loop tuning.
#define CONTROL_CYCLES_FROM_SECONDS(sec) ((uint32_t)((CONTROL_LOOP_HZ) * (sec) + 0.5f))
#define CONTROL_STARTUP_SETTLE_CYCLES CONTROL_CYCLES_FROM_SECONDS(0.020f)	// 20ms
#define POWER_LOSS_BLANKING_CYCLES CONTROL_CYCLES_FROM_SECONDS(4.000f)		// Input power-loss latch startup blanking; restored original protection, requires hardware tuning.
#define POWER_LOSS_DEBOUNCE_CYCLES CONTROL_CYCLES_FROM_SECONDS(0.200f)		// Input power-loss latch debounce; restored original protection, requires hardware tuning.
#define POWER_LOSS_UNLOCK_CYCLES CONTROL_CYCLES_FROM_SECONDS(1.000f)		// Input power-loss latch unlock delay; restored original protection, requires hardware tuning.
#define FF_RATIO_TRIM 1.0f				// Feed-forward trim coefficient; assumed safe value, requires hardware tuning.
#define FF_MIN_BUS_VOLT INV_MIN			// Minimum valid bus voltage for cap_v / in_v feed-forward calculation
#define INPUT_POWER_MARGIN_W 2.0f		// Input power target margin below the CAN power limit.
#define INPUT_POWER_LOOP_KP 0.45f		// Pin outer-loop proportional gain for the current vehicle test.
#define INPUT_POWER_LOOP_KI 0.00005f	// Pin outer-loop integral gain per cycle.
#define INPUT_POWER_LOOP_I_MAX_W 80.0f	// Pin outer-loop integral clamp in watts.
#define CAPV_CHARGE_DERATE_WINDOW 0.50f	// One-way charge-current derating window before CAPV_MAX.
#define CURR_PID_KP 0.015f				// Position PID ratio correction gain; slightly softened for vehicle transient test.
#define CURR_PID_KI 0.00004f			// Position PID integral gain per control cycle; assumed safe value, requires hardware tuning.
#define CURR_PID_CORR_MAX max_volt_ratio	// Maximum current-loop ratio correction; unverified clamp, requires hardware tuning.
#define CURR_PID_CORR_I_MAX max_volt_ratio	// Current-loop integral correction clamp; unverified clamp, requires hardware tuning.
#define CV_HIGH_TARGET_V 32.0f			// Direct PWM-ratio ceiling for unloaded output over-voltage control.
#define CV_HIGH_PID_KP 0.05f			// P-only high-voltage ceiling gain; raw calibrated cap voltage feedback.
#define CV_HIGH_CORR_MAX max_volt_ratio	// Maximum positive CV ratio correction while below 32V.
#define CV_HIGH_CORR_MIN (-max_volt_ratio)	// Negative correction available when raw cap voltage exceeds 32V.
#define CURR_STEP_DISCHARGE 0.75f		// Faster vehicle-test setting: discharge target slew, A/cycle (15 kA/s at 20 kHz).
#define CURR_STEP_CHARGE 0.50f			// Normal charge current target slew, A/cycle; kept conservative for bus stability.
#define CURR_STEP_REGEN 0.60f			// Faster regen test setting: charge-current target slew, A/cycle (12 kA/s at 20 kHz).
#define REGEN_OUTPUT_CURRENT_THRESHOLD_A -0.1f	// Negative output current threshold for braking regen mode; unverified setting, requires hardware tuning.
#define REGEN_OUTPUT_CURRENT_RELEASE_A -0.03f	// Regen release current threshold; unverified setting, requires hardware tuning.
#define REGEN_ENTER_DEBOUNCE_CYCLES 20u	// 1.0ms at 20kHz; faster regen detection test setting.
#define REGEN_EXIT_DEBOUNCE_CYCLES 30u	// 1.5ms at 20kHz; unverified debounce setting, requires hardware tuning.
#define DUTY_RATIO_STEP 0.018f			// Ratio slew limit per control cycle; slightly softened for staged vehicle tuning.

// ====================== Data Structures ======================

// ADC sampling parameter structure
typedef struct
{
	uint8_t elec_type;			// 0: Voltage, 1: Current
	volatile uint16_t valu;		// Raw ADC value
	float offset;				// Calibration offset
	float ratio;				// Calibration ratio
	float result;				// Calculated real value (Voltage in V, Current in A)
} ELEC_INFO_STRUCT;

// Power statistics structure
typedef struct
{
	float U_now;	// Voltage (V)
	float I_now;	// Current (A)
	float P_now;	// Power (W)
} CONTROL_STRUCT;

// Buck-Boost state structure
typedef struct
{
	uint8_t loop_mode;			// 0: Voltage ceiling/charge derating, 1: CC Charge, 2: CC Discharge
	uint16_t max_power_lim;		// Target power limit from CAN (W)
	float ratio;				// Current PWM ratio
	float efficiency;			// Conversion efficiency (%)
} BUCKBOOST_STRUCT;

// ====================== Global Variables ======================
extern ELEC_INFO_STRUCT cap_c, cap_v, in_c, in_v, out_c;
extern CONTROL_STRUCT in, out, cap, dcdc_in;
extern volatile BUCKBOOST_STRUCT buckboost;
extern float ratio_display;
extern float cloop_display;
extern float cap_target_power_display;
extern float ratio_delta_display;
extern uint8_t init_flag;
extern float charge_curr;
extern uint8_t low_side_enabled;
extern volatile uint16_t can_target_power_lim;

// ====================== Function Prototypes ======================
void DCDC_init(void);
void DCDC_deinit(void);
void buckboost_pwm_update(float volt_ratio);
float ADCvalue_to_ELEC(ELEC_INFO_STRUCT *p);
void control_calc(void);

#endif
