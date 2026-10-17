#include "hrtim.h"
#include "LED.h"
#include "power_calculation.h"
#include "PID.h"
#include "CAN_driver.h"
#include "filter.h"

// ====================== Global Variables ======================

// ADC sampling structures
ELEC_INFO_STRUCT cap_c;		// Supercapacitor current
ELEC_INFO_STRUCT cap_v;		// Supercapacitor voltage
ELEC_INFO_STRUCT in_c;		// Input current
ELEC_INFO_STRUCT in_v;		// Input voltage
ELEC_INFO_STRUCT out_c;		// Output (Chassis) current

// Power statistics
CONTROL_STRUCT in, out, cap, dcdc_in;
volatile BUCKBOOST_STRUCT buckboost;

// Display and debugging variables
float ratio_display;
float cloop_display;
float cap_target_power_display;
float ratio_delta_display;
uint8_t init_flag = 0;
float charge_curr = 0.0f;			// Target charge/discharge current (Positive = charge, Negative = discharge)
uint8_t low_side_enabled = 0;		// PWM low-side switch state (0 = OFF, 1 = ON)
#define HRTIM_POWER_OUTPUTS (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

static float clamp_float(float input, float min, float max)
{
	if (input > max) return max;
	if (input < min) return min;
	return input;
}

static float calc_feedforward_ratio(float input_v, float cap_v)
{
	if (input_v <= FF_MIN_BUS_VOLT || cap_v <= 0.0f) {
		return 0.0f;
	}

	return clamp_float((cap_v / input_v) * FF_RATIO_TRIM, 0.0f, max_volt_ratio);
}

static float calc_discharge_current_limit(float cap_voltage)
{
	float x;
	float low_side;
	float scale;

	if (cap_voltage <= CAPV_MIN) {
		return 0.0f;
	}

	if (cap_voltage >= CAPV_DERATE_START) {
		return max_capc_nega;
	}

	x = (cap_voltage - CAPV_MIN) / (CAPV_DERATE_START - CAPV_MIN);
	x = clamp_float(x, 0.0f, 1.0f);
	low_side = 1.0f - x;
	scale = 1.0f - low_side * low_side * low_side;

	return max_capc_nega * scale;
}

static void pid_track_position_output(pid_type_def *pid, float applied_out)
{
	float tracked_iout;

	if (pid == NULL) {
		return;
	}

	applied_out = clamp_float(applied_out, pid->min_out, pid->max_out);
	tracked_iout = applied_out - pid->Pout - pid->Dout;
	tracked_iout = clamp_float(tracked_iout, pid->min_iout, pid->max_iout);

	pid->out = applied_out;
	pid->Iout = tracked_iout;
}

static float smoothed_charge_curr = 0.0f;
static float input_power_iout = 0.0f;

static void clear_current_target_slew(void)
{
	smoothed_charge_curr = 0.0f;
	input_power_iout = 0.0f;
}

static void force_power_stage_off(void)
{
	uint32_t duty_center;

	if (__HAL_RCC_HRTIM1_IS_CLK_ENABLED() == 0U) {
		low_side_enabled = 0;
		return;
	}

	duty_center = HRTIM1->sMasterRegs.MPER / 2u;
	HRTIM1->sTimerxRegs[0].CMP1xR = duty_center;
	HRTIM1->sTimerxRegs[0].CMP3xR = duty_center;
	HRTIM1->sTimerxRegs[1].CMP1xR = duty_center;
	HRTIM1->sTimerxRegs[1].CMP3xR = duty_center;
	HRTIM1->sCommonRegs.ODISR = HRTIM_POWER_OUTPUTS;
	__DSB();
	low_side_enabled = 0;
}

static uint8_t update_regen_state(float out_current)
{
	static uint8_t regen_latched = 0;
	static uint32_t enter_cnt = 0;
	static uint32_t exit_cnt = 0;
	uint8_t enter_cond = (out_current < REGEN_OUTPUT_CURRENT_THRESHOLD_A);
	uint8_t exit_cond = (out_current > REGEN_OUTPUT_CURRENT_RELEASE_A);

	if (regen_latched) {
		enter_cnt = 0;
		if (exit_cond) {
			if (exit_cnt < REGEN_EXIT_DEBOUNCE_CYCLES) {
				exit_cnt++;
			}
			if (exit_cnt >= REGEN_EXIT_DEBOUNCE_CYCLES) {
				regen_latched = 0;
				exit_cnt = 0;
			}
		} else {
			exit_cnt = 0;
		}
	} else {
		exit_cnt = 0;
		if (enter_cond) {
			if (enter_cnt < REGEN_ENTER_DEBOUNCE_CYCLES) {
				enter_cnt++;
			}
			if (enter_cnt >= REGEN_ENTER_DEBOUNCE_CYCLES) {
				regen_latched = 1;
				enter_cnt = 0;
			}
		} else {
			enter_cnt = 0;
		}
	}

	return regen_latched;
}

// ====================== Initialization ======================

void DCDC_init(void)
{
	// ====================== Calibration Selection ======================
	// Change BOARD_NUM in power_calculation.h to switch boards

#if BOARD_NUM == 0
	// ============ No Calibration Mode (Theoretical Values) ============
	// Hardware: in_c/out_c shunt = 3mΩ, cap_c shunt = 1.5mΩ.
	in_v.elec_type = 0;
	in_v.ratio = adc_conv_fact * V_GAIN;
	in_v.offset = 0.0f;

	cap_v.elec_type = 0;
	cap_v.ratio = adc_conv_fact * V_GAIN;
	cap_v.offset = 0.0f;

	cap_c.elec_type = 1;
	cap_c.ratio = adc_conv_fact / I_GAIN / R_SAMP_CAP;
	cap_c.offset = 0.0f;

	in_c.elec_type = 1;
	in_c.ratio = adc_conv_fact / I_GAIN / R_SAMP_INOUT;
	in_c.offset = 0.0f;

	out_c.elec_type = 1;
	out_c.ratio = adc_conv_fact / I_GAIN / R_SAMP_INOUT;
	out_c.offset = 0.0f;

#elif BOARD_NUM == 1
	// ============ Board 1 Calibration Parameters (2026-05-09, Post-repair) ============

	// ============ Board 1 Full Recalibration (2026-05-18) ============
	// Equipment: Multimeter (voltage), Power supply display (input current), Electronic load (output/cap current)
	// NOTE: cap_c shunt = 1.5mΩ (2×3mΩ parallel)
	// Conversion: Voltage: V = ADC * ratio + offset | Current: I = (ADC - I_BIAS) * ratio + offset

	// in_v: 12.05V→ADC=739, 24.1V→ADC=1483
	in_v.elec_type = 0;
	in_v.ratio = (24.1f - 12.05f) / (1483.0f - 739.0f);	// = 0.016196
	in_v.offset = 12.05f - 739.0f * in_v.ratio;			// = 0.081

	// cap_v: 13.71V→ADC=843, 22.6V→ADC=1398
	cap_v.elec_type = 0;
	cap_v.ratio = (22.6f - 13.71f) / (1398.0f - 843.0f);	// = 0.016018
	cap_v.offset = 13.71f - 843.0f * cap_v.ratio;			// = 0.207

	// in_c: 1.354A→ADC=859, 4.341A→ADC=1083
	// Cross-verified: 4.080A→ADC=1063 (err 0.006A), 1.164A→ADC=844 (err 0.010A)
	in_c.elec_type = 1;
	in_c.ratio = (4.341f - 1.354f) / (1083.0f - 859.0f);	// = 0.013335
	in_c.offset = 1.354f - (859.0f - (float)I_BIAS) * in_c.ratio;	// = 0.247

	// out_c: 1.261A→ADC=863, 4.272A→ADC=1089
	out_c.elec_type = 1;
	out_c.ratio = (4.272f - 1.261f) / (1089.0f - 863.0f);	// = 0.013323
	out_c.offset = 1.261f - (863.0f - (float)I_BIAS) * out_c.ratio;	// = 0.102

	// cap_c: 1.129A→ADC=811, 6.768A→ADC=1023 (1.5mΩ shunt)
	// Zero-verified: ADC=767~768 at 0A
	cap_c.elec_type = 1;
	cap_c.ratio = (6.768f - 1.129f) / (1023.0f - 811.0f);	// = 0.026608
	cap_c.offset = 1.129f - (811.0f - (float)I_BIAS) * cap_c.ratio;	// = 0.198

#elif BOARD_NUM == 2
	// ============ Board 2 Full Recalibration (2026-05-18, field-corrected 2026-07-24) ============
	// Equipment: Multimeter (voltage), Power supply display (input current), Electronic load (output/cap current)
	// NOTE: cap_c shunt = 1.5mΩ (2×3mΩ parallel)
	// Conversion: Voltage: V = ADC * ratio + offset | Current: I = (ADC - I_BIAS) * ratio + offset

	// in_v: 12.05V→ADC=739, 24.1V→ADC=1482
	in_v.elec_type = 0;
	in_v.ratio = (24.1f - 12.05f) / (1482.0f - 739.0f);	// = 0.016217
	in_v.offset = 12.05f - 739.0f * in_v.ratio;			// = 0.058

	// cap_v field correction: display 14.000/28.000V -> actual 13.941/27.926V
	cap_v.elec_type = 0;
	cap_v.ratio = 0.0159535845f;
	cap_v.offset = 0.1287817111f;

	// in_c field correction: display 4.010A -> actual 3.981A; zero retained by gain correction
	in_c.elec_type = 1;
	in_c.ratio = 0.0132206571f;
	in_c.offset = 0.1788046224f;

	// out_c field correction: display -0.030/3.910A -> actual 0.000/3.910A
	out_c.elec_type = 1;
	out_c.ratio = 0.0133484749f;
	out_c.offset = 0.0521179750f;

	// cap_c field correction: display 0.040/6.790A -> actual 0.000/6.755A
	cap_c.elec_type = 1;
	cap_c.ratio = 0.0265383351f;
	cap_c.offset = 0.1294229007f;

#elif BOARD_NUM == 3
	// ============ Board 3 Full Recalibration (2026-07-07) ============
	// Zero-first current calibration. Voltage: V = ADC * ratio + offset.
	// Current: I = (ADC - I_BIAS) * ratio + offset.

	// in_v: 12.07V->ADC=738, 24.10V->ADC=1480
	in_v.elec_type = 0;
	in_v.ratio = (24.10f - 12.07f) / (1480.0f - 738.0f);	// = 0.01621294
	in_v.offset = 12.07f - 738.0f * in_v.ratio;				// = 0.10485

	// cap_v: 10.37V->ADC=633, 28.40V->ADC=1739
	cap_v.elec_type = 0;
	cap_v.ratio = (28.40f - 10.37f) / (1739.0f - 633.0f);	// = 0.01630199
	cap_v.offset = 10.37f - 633.0f * cap_v.ratio;			// = 0.05084

	// in_c: zero ADC=763; fitted with 1.274A->854, 4.677A->1108, 2.359A->934, 1.449A->868.
	in_c.elec_type = 1;
	in_c.ratio = 0.01363613f;
	in_c.offset = -((763.0f - (float)I_BIAS) * in_c.ratio);	// = 0.17727

	// out_c: zero ADC=770; fitted with 1.188A->859, 4.609A->1114.
	out_c.elec_type = 1;
	out_c.ratio = 0.01339512f;
	out_c.offset = -((770.0f - (float)I_BIAS) * out_c.ratio);	// = 0.08037

	// cap_c: zero ADC=770; fitted with 5.112A->961, 1.128A->813.
	cap_c.elec_type = 1;
	cap_c.ratio = 0.02673874f;
	cap_c.offset = -((770.0f - (float)I_BIAS) * cap_c.ratio);	// = 0.16043

#elif BOARD_NUM == 4
	// ============ Board 4 Full Recalibration (2026-05-18) ============
	// Equipment: Multimeter (voltage), Power supply display (input current), Electronic load (output/cap current)
	// Method: Two-point linear regression for all 5 channels
	// NOTE: cap_c shunt resistor changed from 3mΩ to 1.5mΩ (all boards)
	// Conversion formula — Voltage: V = ADC * ratio + offset
	//                      Current: I = (ADC - I_BIAS) * ratio + offset

	// --- Voltage channels ---
	// in_v: 12.05V→ADC=744, 24.1V→ADC=1493
	in_v.elec_type = 0;
	in_v.ratio = (24.1f - 12.05f) / (1493.0f - 744.0f);	// = 0.016088
	in_v.offset = 12.05f - 744.0f * in_v.ratio;			// = 0.081

	// cap_v: 13.55V→ADC=841, 22.5V→ADC=1400
	cap_v.elec_type = 0;
	cap_v.ratio = (22.5f - 13.55f) / (1400.0f - 841.0f);	// = 0.016011
	cap_v.offset = 13.55f - 841.0f * cap_v.ratio;			// = 0.085

	// --- Current channels: I = (ADC - 776) * ratio + offset ---
	// in_c: 1.352A(电源箱)→ADC=863, 4.769A→ADC=1117
	// Cross-verified: 3.988A→ADC=1059 (err 0.001A), 1.147A→ADC=847 (err 0.010A)
	in_c.elec_type = 1;
	in_c.ratio = (4.769f - 1.352f) / (1117.0f - 863.0f);	// = 0.013445
	in_c.offset = 1.352f - (863.0f - (float)I_BIAS) * in_c.ratio;	// = 0.182

	// out_c: 1.261A(负载箱)→ADC=868, 4.708A→ADC=1125
	// Zero-verified: ADC=774~775 at 0A
	out_c.elec_type = 1;
	out_c.ratio = (4.708f - 1.261f) / (1125.0f - 868.0f);	// = 0.013412
	out_c.offset = 1.261f - (868.0f - (float)I_BIAS) * out_c.ratio;	// = 0.027

	// cap_c: 1.120A(负载箱)→ADC=812, 6.692A→ADC=1023 (1.5mΩ shunt!)
	// Zero-verified: ADC=770~773 at 0A
	cap_c.elec_type = 1;
	cap_c.ratio = (6.692f - 1.120f) / (1023.0f - 812.0f);	// = 0.026408
	cap_c.offset = 1.120f - (812.0f - (float)I_BIAS) * cap_c.ratio;	// = 0.169

#elif BOARD_NUM == 5 || BOARD_NUM == 6
	#error "BOARD_NUM 5 and 6 are pending calibration and must not be used."

#else
	#error "Undefined BOARD_NUM! Use 0 for theory or a calibrated board from 1 to 4."
#endif

	buckboost.max_power_lim = can_target_power_lim;
	init_flag = 0;

	// [SAFETY] Clear compare registers before starting PWM to ensure 0% duty cycle on startup
	force_power_stage_off();

	// [ABSOLUTE MANDATORY]: Do not output waveforms and enable system immediately upon power up!
	// Keep the waveform generator disabled and wait for control_calc to verify a safe 24V input before hot-starting.
	// Only start the underlying counters; the pins will remain in a safe, silent (Idle/Stop) state.
	HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_TIMER_B | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_MASTER);



	// PID Initialization
	// Feed-forward supplies the base cap_v/in_v ratio; position PID only outputs ratio correction.
	// These gains are assumed safe starting values and require hardware tuning.
	PID_init(&curr_loop, PID_POSITION, CURR_PID_KP, CURR_PID_KI, 0,
	         CURR_PID_CORR_MAX, CURR_PID_CORR_I_MAX,
	         -CURR_PID_CORR_MAX, -CURR_PID_CORR_I_MAX);
	// P-only 32V ceiling: no integral means no hidden CV windup while the voltage is below its target.
	PID_init(&volt_high_loop, PID_POSITION, CV_HIGH_PID_KP, 0.0f, 0.0f,
	         CV_HIGH_CORR_MAX, 0.0f,
	         CV_HIGH_CORR_MIN, 0.0f);
	clear_current_target_slew();

#if LOW_PASS_FILTTER == 1
	// 控制环路采样频率 Fs = CONTROL_LOOP_HZ (TIM1 currently 20kHz, period = 50us)
	// 各通道截止频率 Fc 配置 (参数即真实物理值，无需换算)：
	// - lpf[0] Fc=50Hz   输入电压，变化极缓慢，重滤波保证稳态纯净
	// - lpf[1] Fc=1kHz   输入电流，功率闭环核心反馈，高带宽以缩短环路延迟
	// - lpf[2] Fc=1kHz   底盘输出电流，功率平衡基础项通道，需快速跟随瞬态拉载
	// - lpf[3] Fc=50Hz   电容电压，变化极缓慢，重滤波保证稳态纯净
	// - lpf[4] Fc=1kHz   电容电流，PID 反馈通道，高带宽避免控制相位滞后
	low_filter_init(&lpf[0], CONTROL_LOOP_HZ, 50);
	low_filter_init(&lpf[1], CONTROL_LOOP_HZ, 1e3);
	low_filter_init(&lpf[2], CONTROL_LOOP_HZ, 1e3);
	low_filter_init(&lpf[3], CONTROL_LOOP_HZ, 50);
	low_filter_init(&lpf[4], CONTROL_LOOP_HZ, 1e3);
#endif
}

void DCDC_deinit(void)
{
	buckboost.max_power_lim = can_target_power_lim;
	init_flag = 0;
	// Safely shutdown all outputs
	force_power_stage_off();
}

// ====================== Core Control Loop ======================

// Global hardware physical power-loss interlock flag
uint8_t hard_power_loss_latch = 0;

void control_calc(void)
{
	IWDG->KR = 0xAAAA;  // [SAFETY] Feed hardware watchdog — proves control loop is alive

	static float inv, inc, outc, capv, capc;
	static float ratio_pass = 0, cloop = 0;
	static float ratio_last = 0;  // Soft-start limiter state (synchronized during initialization)
	uint8_t request_pwm_enable_this_cycle = 0;
	uint8_t cv_ratio_limited = 0;

	// Fetch the local power limit to prevent data races with the CAN receive interrupt
	buckboost.max_power_lim = can_target_power_lim;

	// ---- ADC Sampling and Filtering ----
	inv = ADCvalue_to_ELEC(&in_v);
	inc = ADCvalue_to_ELEC(&in_c);
	outc = ADCvalue_to_ELEC(&out_c);
	capv = ADCvalue_to_ELEC(&cap_v);
	capc = ADCvalue_to_ELEC(&cap_c);
	// Preserve the calibrated raw capacitor voltage for the fast 32V CV ceiling.
	// The normal control/telemetry path below still uses the existing 50Hz filtered voltage.
	float capv_raw = capv;

#if LOW_PASS_FILTTER == 1
	lpf[0].Input = inv;
	lpf[1].Input = inc;
	lpf[2].Input = outc;
	lpf[3].Input = capv;
	lpf[4].Input = capc;
	for (uint8_t i = 0; i < 5; i++) {
		low_filter_calc(&lpf[i]);
	}
	inv = lpf[0].Output;
	inc = lpf[1].Output;
	outc = lpf[2].Output;
	capv = lpf[3].Output;
	capc = lpf[4].Output;
#endif

	// ---- Power Calculation (Simplified and high precision refactored) ----
	in.U_now = inv;
	// Discard noisy in.I_now; directly read the highly precise output current measure
	out.U_now = inv;				// Output voltage ≈ Input voltage (passthrough)
	out.I_now = outc; // Output current from ADC
	out.P_now = out.U_now * out.I_now;

	cap.U_now = capv; // Capacitor voltage from ADC
	cap.I_now = capc; // Capacitor current from ADC
	
	// KCL fallback for cap_c ADC saturation during heavy discharge
	// INA240A1 bias point ~769 counts with 1.5mΩ cap shunt → max measurable discharge is about -19A to -20A.
	// Normal -14.5A discharge should remain measurable; this fallback is a last-resort guard for ADC floor/noise faults.
	// Solution: When cap_c ADC approaches saturation, estimate I_cap from calibrated in_c and out_c
	// using power conservation: P_cap = P_in - P_out, thus I_cap = (P_in - P_out) / V_cap
	#define CAPC_ADC_SAT_THRESHOLD  50		// ADC < 50 = approaching saturation (1.5mΩ shunt: ~20A max, safety net only)
	if (cap_c.valu < CAPC_ADC_SAT_THRESHOLD && capv > 2.0f) {
		// cap_c ADC is near zero → sensor saturated → use KCL estimation
		float p_chassis = inv * outc;		// P_out = V_in × I_out (passthrough)
		float p_input = inv * inc;			// P_in  = V_in × I_in
		float i_cap_est = (p_input - p_chassis) / capv;
		cap.I_now = i_cap_est;				// Override with KCL estimate
	}
	cap.P_now = cap.U_now * cap.I_now;

	// New rule: Restore real ADC input current display to address user concerns
	in.I_now = inc; 
	in.P_now = in.I_now * in.U_now;
	
	// Synchronize telemetry variables with display data structures for CAN feedback
	dcdc_in.U_now = in.U_now;
	dcdc_in.I_now = in.I_now;
	dcdc_in.P_now = in.P_now;
	buckboost.efficiency = 100.0f; // Perfect physical passthrough, assume efficiency is extremely high
	float feedforward_ratio = calc_feedforward_ratio(in.U_now, cap.U_now);
	uint8_t regen_active = update_regen_state(out.I_now);
	
	// ---- Input-power outer loop: Pin -> cap current target ----
	// Pin_target - Pout is the power-balance base term. The outer PI only corrects the cap-current target;
	// it never drives duty directly. Duty feed-forward is Vcap/Vin, followed by the cap-current PID correction.
	float cap_voltage_for_power = (cap.U_now > 0.5f) ? cap.U_now : 0.5f;
	float pin_target_power = (float)buckboost.max_power_lim - INPUT_POWER_MARGIN_W;
	float cap_power_base = 0.0f;
	float input_power_p_term = 0.0f;
	float pin_error = 0.0f;
	float input_power_iout_candidate = input_power_iout;
	float target_cap_power = 0.0f;
	uint8_t input_power_loop_active = 0;
	uint8_t cap_charge_upper_limited = 0;
	uint8_t cap_discharge_lower_limited = 0;
	uint8_t cap_voltage_limited = 0;
	if (pin_target_power < 0.0f) {
		pin_target_power = 0.0f;
	}

	if (in.U_now > 10.0f) {
		input_power_loop_active = 1;
		pin_error = pin_target_power - in.P_now;
		cap_power_base = pin_target_power - out.P_now;
		input_power_iout_candidate = clamp_float(input_power_iout + INPUT_POWER_LOOP_KI * pin_error,
		                                           -INPUT_POWER_LOOP_I_MAX_W,
		                                            INPUT_POWER_LOOP_I_MAX_W);
		input_power_p_term = INPUT_POWER_LOOP_KP * pin_error;
		target_cap_power = cap_power_base + input_power_p_term + input_power_iout_candidate;
		charge_curr = target_cap_power / cap_voltage_for_power;
	} else {
		input_power_iout = 0.0f;
		charge_curr = 0.0f;  // Do not operate if bus voltage is abnormal
	}

	// === NEW: Equivalent Series Resistance (ESR) voltage drop compensation for high-current supercapacitor discharge ===
	// Resolves the issue entirely where high-power discharge creates severe internal voltage drops, causing the terminal voltage to drop to 3V even if the capacitor still has 8V internally (triggering an unjust shutdown protection)
	// Through "Terminal Voltage - Discharge Current * Estimated Total ESR (ESR_COMP_OHM)", we back-trace the true internal chemical voltage of the supercapacitor!
	// During discharge I_now is negative, so subtracting a negative value essentially adds the voltage drop back.
	float vcap_real = cap.U_now - cap.I_now * ESR_COMP_OHM;

	// Removed the 12V limitation. Discharge is allowed as long as CAPV_MIN is reached. 
	// If the internal real voltage drops below CAPV_MIN, forcefully stop discharge and lock it up
	static uint8_t allow_discharge = 0;
	if (cap.U_now >= CAPV_MIN) {
		allow_discharge = 1;
	} else {
		// Absolutely do NOT use vcap_real to determine bottom limit! Because in constant-power discharge, lower voltage draws higher current,
		// the back-traced compensation voltage drop (I*R) will fall even faster than the terminal voltage itself! This causes vcap_real to NEVER hit 5V!
		allow_discharge = 0;
	}
	
	// If discharge is not allowed, forcefully intercept any attempt making charge_curr < 0, stopping it from drawing load outward
	if (allow_discharge == 0 && charge_curr < 0.0f) {
		charge_curr = 0.0f;
		cap_discharge_lower_limited = 1;
	}

	// Bi-directional physical limits for charge/discharge (Protects supercapacitor and MOSFETs)
	// Regen limit is higher only when braking regen is detected. Requires hardware tuning.
	float charge_current_limit = regen_active ? max_capc_posi_regen : max_capc_posi;
	float discharge_current_limit = calc_discharge_current_limit(cap.U_now);
	if (cap.U_now >= CAPV_MAX) {
		charge_current_limit = 0.0f;
		cap_voltage_limited = 1;
	} else if (cap.U_now > (CAPV_MAX - CAPV_CHARGE_DERATE_WINDOW)) {
		float charge_scale = (CAPV_MAX - cap.U_now) / CAPV_CHARGE_DERATE_WINDOW;
		charge_current_limit *= clamp_float(charge_scale, 0.0f, 1.0f);
		cap_voltage_limited = 1;
	}
	if (charge_curr > charge_current_limit) {
		charge_curr = charge_current_limit;		// Positive forward charging physical limit
		cap_charge_upper_limited = 1;
	} else if (charge_curr < discharge_current_limit) {
		charge_curr = discharge_current_limit;		// Negative reverse discharging physical limit
		cap_discharge_lower_limited = 1;
	}

	// Conditional integration: temporary current limits must not be written into I in one cycle.
	// Freeze only when the integral update would push farther into the active limit;
	// keep integrating when the error direction helps the command leave saturation.
	if (input_power_loop_active) {
		uint8_t pushes_further_into_limit =
			(cap_charge_upper_limited && pin_error > 0.0f) ||
			(cap_discharge_lower_limited && pin_error < 0.0f);
		if (!pushes_further_into_limit) {
			input_power_iout = input_power_iout_candidate;
		}
	}

	// ====================== 目标电流变化率限速环 ======================
	// 作用：平滑 PID 目标电流的阶跃变化，防止 PID P项产生巨大误差导致占空比尖峰和电流过冲。
	// 硬件安全由下游的占空比限速环 (max_rise) 兜底，此处主要影响瞬态响应平滑度。
	//
	// ┌─ 关键元器件 ──────────────────────────────────────────────────┐
	// │ 电感: SER2918H-103KL (10μH, Isat=28A, Irms=28A)            │
	// │ MOSFET: CSD18540Q5BT (60V, Id_amb=29A, Id_pulse=400A)      │
	// │ 电容放电电流上限: max_capc_nega = -14.5A                    │
	// │ 普通充电上限: +5A, 急刹回收上限: +10A                        │
	// └──────────────────────────────────────────────────────────────┘
	//
	// 公式: 物理斜率 (A/s) = curr_step × CONTROL_LOOP_HZ
	//       满载响应时间 = max_capc / curr_step / CONTROL_LOOP_HZ
	//
	// ┌─ 放电方向 (curr_step_discharge) ─────────────────────────────┐
	// │ 当前值: 0.75f → 15000 A/s → 0→-14.5A 需 0.97ms             │
	// │ 该值需要结合电感峰值和输入功率瞬态进行实物调试                │
	// └──────────────────────────────────────────────────────────────┘
	// ┌─ 充电方向 (curr_step_charge) ────────────────────────────────┐
	// │ 普通充电: 0.50f → 10000 A/s → 0→+5A 需 0.50ms              │
	// │ 急刹回收: 0.60f → 12000 A/s → 0→+10A 需 0.83ms             │
	// │ 该值按用户要求设置，需观察母线电压、电感电流和MOS温升          │
	// └──────────────────────────────────────────────────────────────┘
	float curr_step;
	if (charge_curr < smoothed_charge_curr) {
		curr_step = CURR_STEP_DISCHARGE;   // 放电方向：极速响应底盘瞬态拉载
	} else if (regen_active) {
		curr_step = CURR_STEP_REGEN;       // 急刹回收：快速吸收底盘回灌能量，需要实物调试
	} else {
		curr_step = CURR_STEP_CHARGE;      // 普通充电方向：保护电池总线，需要实物调试
	}
	if (smoothed_charge_curr < charge_curr - curr_step) {
		smoothed_charge_curr += curr_step;
	} else if (smoothed_charge_curr > charge_curr + curr_step) {
		smoothed_charge_curr -= curr_step;
	} else {
		smoothed_charge_curr = charge_curr;
	}
	cap_target_power_display = smoothed_charge_curr * cap.U_now;

	// ====================== Mode Branches ======================

#if DCDC_OUTPUT == 1
	// === Mode 1: Input Side Buck Half-Bridge Hardware Test (Test TA1, TA2) ===
	// WARNING: Strictly ZERO load and NO capacitor connected in this mode!
	// Hookup: Apply 24V to INPUT, leave CAP floating. Measure CAP voltage with a multimeter.
	{
		ratio_pass = 0.5f; // Purely for OLED display
		
		// Force TA side to 50% PWM Duty
		uint32_t duty_half = HRTIM1->sMasterRegs.MPER / 2u;
		HRTIM1->sTimerxRegs[0].CMP1xR = (HRTIM1->sMasterRegs.MPER - duty_half) / 2u;
		HRTIM1->sTimerxRegs[0].CMP3xR = (HRTIM1->sMasterRegs.MPER + duty_half) / 2u;
		
		// TB side 95% pass-through (TB1 conducting fully opens measuring path, eliminates peak rectifying effect)
		uint32_t tb_duty = HRTIM1->sMasterRegs.MPER * 0.95f;
		HRTIM1->sTimerxRegs[1].CMP1xR = (HRTIM1->sMasterRegs.MPER - tb_duty) / 2u;
		HRTIM1->sTimerxRegs[1].CMP3xR = (HRTIM1->sMasterRegs.MPER + tb_duty) / 2u;
		
		if (low_side_enabled == 0) {
			HRTIM1->sCommonRegs.ICR = HRTIM_ICR_FLT1C | HRTIM_ICR_FLT2C | HRTIM_ICR_FLT3C | HRTIM_ICR_FLT4C | HRTIM_ICR_FLT5C | HRTIM_ICR_SYSFLTC;
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
			low_side_enabled = 1;
		}
		ratio_display = 1.0f; // OLED identifier for Mode 1
		// Block buckboost_pwm_update at the bottom
		return;
	}

#elif DCDC_OUTPUT == 2
	// === Mode 2: Capacitor-side Boost Half-bridge Hardware Test (Tests TB1, TB2) ===
	// WARNING: DO NOT CONNECT ANY LOAD IN THIS MODE! Empty load testing ONLY! 
	// Setup: Connect 24V supply to CAP, leave INPUT floating. Use a multimeter to measure voltage at the INPUT terminal.
	{
		ratio_pass = 0.5f; 
		
		// Force TB side to 50% duty cycle
		uint32_t duty_half = HRTIM1->sMasterRegs.MPER / 2u;
		HRTIM1->sTimerxRegs[1].CMP1xR = (HRTIM1->sMasterRegs.MPER - duty_half) / 2u;
		HRTIM1->sTimerxRegs[1].CMP3xR = (HRTIM1->sMasterRegs.MPER + duty_half) / 2u;
		
		// TA side remains 95% constantly open (TA1 conducting, bridging the bi-directional measurement path from inductor to INPUT)
		uint32_t ta_duty = HRTIM1->sMasterRegs.MPER * 0.95f;
		HRTIM1->sTimerxRegs[0].CMP1xR = (HRTIM1->sMasterRegs.MPER - ta_duty) / 2u;
		HRTIM1->sTimerxRegs[0].CMP3xR = (HRTIM1->sMasterRegs.MPER + ta_duty) / 2u;
		
		if (low_side_enabled == 0) {
			HRTIM1->sCommonRegs.ICR = HRTIM_ICR_FLT1C | HRTIM_ICR_FLT2C | HRTIM_ICR_FLT3C | HRTIM_ICR_FLT4C | HRTIM_ICR_FLT5C | HRTIM_ICR_SYSFLTC;
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2);
			low_side_enabled = 1;
		}
		ratio_display = 2.0f; // OLED identifier for Mode 2
		// Block buckboost_pwm_update at the bottom
		return;
	}

#elif DCDC_OUTPUT == 3
	// === Mode 3: Input-power outer loop + cap-current inner loop (Normal Operation) ===
	if (init_flag == 0) {
		static uint32_t settle_cnt = 0;
		settle_cnt++;
		ratio_pass = 0;
		if (settle_cnt > CONTROL_STARTUP_SETTLE_CYCLES) {
			PID_clear(&curr_loop);
			PID_clear(&volt_high_loop);
			
			float init_ratio = feedforward_ratio;
			
			pid_track_position_output(&curr_loop, 0.0f);
			clear_current_target_slew();
			
			ratio_pass = init_ratio;
			ratio_last = init_ratio; 
			init_flag = 1;
		}
	} else {
		// Underlying interception and soft-restart recovery during normal operation mode
			// Sync software state with HRTIM hardware OENR register
			// EMI/ESD can trigger HRTIM fault latch, silently killing PWM outputs
			// while low_side_enabled remains 1 → recovery logic never triggers → permanent lockout!
			// Periodically read hardware register and force-sync the software flag.
			// Restore PWM and seamless hot-start when bus voltage is healthy and no input power-loss latch is active.
			if (low_side_enabled == 1) {
				uint32_t enabled_outputs = HRTIM1->sCommonRegs.OENR & HRTIM_POWER_OUTPUTS;
				if (enabled_outputs != HRTIM_POWER_OUTPUTS) {
					ratio_pass = 0.0f;
					ratio_last = 0.0f;
					PID_clear(&curr_loop);
					PID_clear(&volt_high_loop);
					clear_current_target_slew();
					force_power_stage_off();
				}
			}
			if (low_side_enabled == 0 && hard_power_loss_latch == 0 && in.U_now > INV_MIN) {
				// Clear HRTIM hardware fault latch before re-enabling outputs
				// EMI/ESD (e.g. hand touching board) can trigger HRTIM fault protection,
				// which locks outputs at the peripheral level. Without clearing, outputs
				// stay dead even after software reset.
				float restart_ratio = feedforward_ratio;
				if (restart_ratio > max_volt_ratio) restart_ratio = max_volt_ratio;
				if (restart_ratio < 0.0f) restart_ratio = 0.0f;

				pid_track_position_output(&curr_loop, 0.0f);
				PID_clear(&volt_high_loop);
				clear_current_target_slew();
				ratio_last = restart_ratio;
				ratio_pass = restart_ratio;
				request_pwm_enable_this_cycle = 1;
			}
			
			curr_loop.max_out = CURR_PID_CORR_MAX;
			curr_loop.min_out = -CURR_PID_CORR_MAX;
			
			// Current loop is the normal controller. The raw-voltage CV loop is only a one-way
			// ratio ceiling: below 32V its allowed ratio is higher and it does not interfere.
			float curr_corr = PID_calc(&curr_loop, cap.I_now, smoothed_charge_curr);
			cloop = feedforward_ratio + curr_corr;
			float cv_corr = PID_calc(&volt_high_loop, capv_raw, CV_HIGH_TARGET_V);
			float vloop_high = feedforward_ratio + cv_corr;
			if (vloop_high < cloop) {
				ratio_pass = vloop_high;
				cv_ratio_limited = 1;
				buckboost.loop_mode = 0;
			} else {
				ratio_pass = cloop;
				buckboost.loop_mode = (cap_voltage_limited && smoothed_charge_curr >= -0.1f) ? 0 : ((smoothed_charge_curr < -0.1f) ? 2 : 1);
			}
		}
	
	// Absolute physical safety boundary limits
	uint8_t force_pwm_off = (in.U_now <= INV_MIN || hard_power_loss_latch == 1);
	uint8_t ratio_limited = cv_ratio_limited;
	if (force_pwm_off) {
		ratio_pass = 0.0f;
		ratio_limited = 1;
	}
	if (ratio_pass > max_volt_ratio) {
		ratio_pass = max_volt_ratio;
		ratio_limited = 1;
	}
	if (ratio_pass < 0.0f) {
		ratio_pass = 0.0f;
		ratio_limited = 1;
	}

	// ====================== 占空比变化率限速环 (硬件安全最后防线) ======================
	// 作用：限制占空比每个控制周期的最大变化量，防止电感电流单步过冲超过饱和点。
	// 约束：max_rise 必须严格等于 max_fall，非对称会导致整流效应使低端管误触发啸叫。
	//
	// ┌─ 关键元器件 ──────────────────────────────────────────────────┐
	// │ 电感: SER2918H-103KL  L=10μH, Isat=28A (感量跌20%的起始点) │
	// │ MOSFET: CSD18540Q5BT  Vds=60V, Id(环境温度25°C)=29A        │
	// │ 输入电压: Vin=24V, 控制周期: Ts=50μs (20kHz)               │
	// │ 满载稳态电流: 14.5A (max_capc_nega)                         │
	// │ HRTIM周期: 34000 ticks, 死区: 40 ticks (118ns, 硬件强制)   │
	// └──────────────────────────────────────────────────────────────┘
	//
	// 推导公式:
	//   单步电流增量 ΔI = Vin × max_rise × Ts / L
	//                    = 24 × max_rise × 50μs / 10μH
	//                    = max_rise × 120 (A)
	//
	//   理论极限 = (Isat - I_steady_max) / 120 = (28-14.5)/120 = 0.112f
	//
	// 当前值: DUTY_RATIO_STEP = 0.018f -> dI~=2.16A/step, 0->100% about 2.8ms.
	// 该值按用户要求设置，接近极限实验档，必须实物调试母线、电感电流、MOS温升和振铃。
	{
		float max_rise = DUTY_RATIO_STEP;   // 对称上升限制，按用户要求设为0.018，必须实物调试
		float max_fall = DUTY_RATIO_STEP;   // 对称下降限制 (必须等于 max_rise)
		if (force_pwm_off) {
			ratio_pass = 0.0f;
			ratio_last = 0.0f;
			PID_clear(&curr_loop);
			PID_clear(&volt_high_loop);
			clear_current_target_slew();
			force_power_stage_off();
		} else {
			if (ratio_pass > ratio_last + max_rise) {
				ratio_pass = ratio_last + max_rise;
				ratio_limited = 1;
			} else if (ratio_pass < ratio_last - max_fall) {
				ratio_pass = ratio_last - max_fall;
				ratio_limited = 1;
			}
		}
		
		// Anti-windup only when the command was physically limited by clamps or duty slew.
		if (ratio_limited && !force_pwm_off) {
			float applied_corr = ratio_pass - feedforward_ratio;
			float curr_track_corr = applied_corr;
			if (smoothed_charge_curr >= 0.0f && curr_track_corr < 0.0f) {
				curr_track_corr = 0.0f; // Do not drag the current loop into discharge while its target is charge/zero.
			}
			pid_track_position_output(&curr_loop, curr_track_corr);
		}
		
		ratio_last = ratio_pass;
	}
	
	// Original input power-loss latch.
	// Purpose: if the input source appears disconnected while the output is still being fed by the capacitor,
	// cut the power stage and wait until the input bus is strong again before auto-recovering.
	static uint32_t power_loss_debounce = 0;
	static uint32_t startup_blanking = POWER_LOSS_BLANKING_CYCLES;
	if (init_flag == 1 && startup_blanking > 0) {
		startup_blanking--;
		power_loss_debounce = 0;
	}

	uint8_t input_power_missing = (in.P_now < 2.0f && in.P_now > -2.0f);
	uint8_t output_still_powered = ((out.P_now - in.P_now) > 1.5f);
	uint8_t input_voltage_lost = (in.U_now < INV_MIN);
	if (startup_blanking == 0 && ((output_still_powered && input_power_missing) || input_voltage_lost)) {
		if (power_loss_debounce < POWER_LOSS_DEBOUNCE_CYCLES) {
			power_loss_debounce++;
		}
		if (power_loss_debounce >= POWER_LOSS_DEBOUNCE_CYCLES) {
			power_loss_debounce = POWER_LOSS_DEBOUNCE_CYCLES;
			hard_power_loss_latch = 1;

			ratio_pass = 0.0f;
			ratio_last = 0.0f;
			PID_clear(&curr_loop);
			PID_clear(&volt_high_loop);
			clear_current_target_slew();
			force_power_stage_off();
		}
	} else {
		power_loss_debounce = 0;
	}

	static uint32_t unlock_delay = 0;
	if (hard_power_loss_latch == 1 && in.U_now > (INV_MIN + INV_RECOVERY_HYSTERESIS_V)) {
		if (unlock_delay < POWER_LOSS_UNLOCK_CYCLES) {
			unlock_delay++;
		}
		if (unlock_delay >= POWER_LOSS_UNLOCK_CYCLES) {
			hard_power_loss_latch = 0;
			unlock_delay = 0;
		}
	} else {
		unlock_delay = 0;
	}

	// User-defined LED indicator status logic
	if (hard_power_loss_latch == 1) {
		LED_SetColor(RED_COLOR);
	} else if (allow_discharge == 0 && vcap_real <= (CAPV_MIN + 0.5f) && charge_curr <= 0.1f) {
		LED_SetColor(RED_COLOR);
	} else if (charge_curr < -0.1f) {
		LED_SetColor(BLUE_COLOR);
	} else if (charge_curr > 0.1f) {
		LED_SetColor(GREEN_COLOR);
	} else {
		if (allow_discharge == 0) LED_SetColor(RED_COLOR);
		else LED_SetColor(GREEN_COLOR);
	}

	// Final hardware update after all protection decisions in this control cycle.
	uint8_t final_force_pwm_off = (force_pwm_off || hard_power_loss_latch == 1 || in.U_now <= INV_MIN);
	if (final_force_pwm_off) {
		ratio_pass = 0.0f;
		ratio_last = 0.0f;
		PID_clear(&curr_loop);
		PID_clear(&volt_high_loop);
		clear_current_target_slew();
		force_power_stage_off();
	} else {
		buckboost_pwm_update(ratio_pass);
		if (request_pwm_enable_this_cycle && ratio_pass > 0.0f) {
			HRTIM1->sCommonRegs.ICR = HRTIM_ICR_FLT1C | HRTIM_ICR_FLT2C | HRTIM_ICR_FLT3C | HRTIM_ICR_FLT4C | HRTIM_ICR_FLT5C | HRTIM_ICR_SYSFLTC;
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_POWER_OUTPUTS);
			low_side_enabled = 1;
		}
	}
	buckboost.ratio = ratio_pass;
	cloop_display = cloop;
	ratio_delta_display = ratio_pass - feedforward_ratio;
	ratio_display = ratio_pass;
#endif
}

// ====================== PWM Update ======================

// Replenish missing PWM parameter macros (possibly lost in old versions or headers)
#ifndef DP_PWM_PER
#define DP_PWM_PER    ((float)(HRTIM1->sMasterRegs.MPER))
#endif
#ifndef shift_duty
#define shift_duty    1.0f
#endif
#ifndef shift_duty_2
#define shift_duty_2  (2.0f * MAX_PWM_RATIO)
#endif
#ifndef MAX_PWM_CMP
#define MAX_PWM_CMP   (uint32_t)(DP_PWM_PER * MAX_PWM_RATIO)
#endif
#ifndef MIN_PWM_CMP
#define MIN_PWM_CMP   (uint32_t)(DP_PWM_PER * MIN_PWM_RATIO)
#endif

void buckboost_pwm_update(float volt_ratio)
{
	volatile uint32_t buck_duty = 0;
	volatile uint32_t boost_duty = 0;

	// Absolute safety bottom-line: ratio=0 means all four power outputs are disabled, not only 0% buck duty.
	if (volt_ratio <= 0.0f) {
		force_power_stage_off();
		return;
	}

	// Ideal relation: Vcap / Vin = D_TA_top / D_TB_top.
	// Keep one half-bridge at MAX_PWM_RATIO and calculate the other one from volt_ratio,
	// so the 0.97 bootstrap margin does not create a fake 1:1 plateau near ratio=1.
	float buck_ratio = MAX_PWM_RATIO;
	float boost_ratio = MAX_PWM_RATIO;
	if (volt_ratio <= 1.0f) {
		buck_ratio = volt_ratio * MAX_PWM_RATIO;
		boost_ratio = MAX_PWM_RATIO;
	} else {
		buck_ratio = MAX_PWM_RATIO;
		boost_ratio = MAX_PWM_RATIO / volt_ratio;
	}

	if (buck_ratio > MAX_PWM_RATIO) buck_ratio = MAX_PWM_RATIO;
	if (buck_ratio < MIN_PWM_RATIO) buck_ratio = MIN_PWM_RATIO;
	if (boost_ratio > MAX_PWM_RATIO) boost_ratio = MAX_PWM_RATIO;
	if (boost_ratio < MIN_PWM_RATIO) boost_ratio = MIN_PWM_RATIO;
	buck_duty = shift_duty * buck_ratio * DP_PWM_PER;
	boost_duty = shift_duty * boost_ratio * DP_PWM_PER;

	// Update front half-bridge (Buck Side)
	HRTIM1->sTimerxRegs[0].CMP1xR = (HRTIM1->sMasterRegs.MPER - buck_duty) / 2u;
	HRTIM1->sTimerxRegs[0].CMP3xR = (HRTIM1->sMasterRegs.MPER + buck_duty) / 2u;
	
	// Update rear half-bridge (Boost Side)
	HRTIM1->sTimerxRegs[1].CMP1xR = (HRTIM1->sMasterRegs.MPER - boost_duty) / 2u;
	HRTIM1->sTimerxRegs[1].CMP3xR = (HRTIM1->sMasterRegs.MPER + boost_duty) / 2u;
}

// ====================== ADC Conversion ======================

float ADCvalue_to_ELEC(ELEC_INFO_STRUCT *p)
{
	switch (p->elec_type) {
	case 0: // Voltage: result = ADC * ratio + offset
		p->result = (p->valu) * p->ratio + p->offset;
		break;
	case 1: // Current: result = (ADC - zero_bias) * ratio + offset
		p->result = ((p->valu) - I_BIAS) * p->ratio + p->offset;
		break;
	}
	return p->result;
}


