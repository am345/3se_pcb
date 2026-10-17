#include "vofa.h"

#include "power_calculation.h"
#include "usart.h"
#include "stdio.h"

#define VOFA_TX_BUF_SIZE 160u

static char vofa_tx_buf[VOFA_TX_BUF_SIZE];

static int32_t scaled_i32(float value, float scale)
{
	float scaled = value * scale;

	if (scaled > 2147483000.0f) {
		return 2147483000;
	}
	if (scaled < -2147483000.0f) {
		return -2147483000;
	}

	return (int32_t)(scaled >= 0.0f ? (scaled + 0.5f) : (scaled - 0.5f));
}

void VOFA_SendTelemetry(void)
{
	int len;

	// VOFA FireWater/CSV channels:
	// 0 Pin(W), 1 Pout(W), 2 Vcap*100, 3 Pcap_actual(W),
	// 4 Vcap*100, 5 Icap*100, 6 Iin*100, 7 Iout*100, 8 Vin*100, 9 Pcap_actual(W)
	len = snprintf(vofa_tx_buf, sizeof(vofa_tx_buf),
	               "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\r\n",
	               (long)scaled_i32(in.P_now, 1.0f),
	               (long)scaled_i32(out.P_now, 1.0f),
	               (long)scaled_i32(cap.U_now, 100.0f),
	               (long)scaled_i32(cap.P_now, 1.0f),
	               (long)scaled_i32(cap.U_now, 100.0f),
	               (long)scaled_i32(cap.I_now, 100.0f),
	               (long)scaled_i32(in.I_now, 100.0f),
	               (long)scaled_i32(out.I_now, 100.0f),
	               (long)scaled_i32(in.U_now, 100.0f),
	               (long)scaled_i32(cap.P_now, 1.0f));

	if (len <= 0 || len >= (int)sizeof(vofa_tx_buf)) {
		return;
	}

	(void)USART2_TryStartTx((const uint8_t *)vofa_tx_buf, (uint16_t)len);
}
