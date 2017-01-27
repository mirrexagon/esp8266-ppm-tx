//! RC receiver that outputs PPM.
//!
//! 16 bits per channel, default 8 channels.
//!
//! TODO:
//! - Make UART bridge for telemetry.
//! - Allow setting of a failsafe position as well as normal failsafe?
//!


// --- Include --- //
#include <stdint.h>
#include <limits.h>

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"

#include "espmissingincludes.h"

#include "driver/uart.h"

#include "fw_config.h"

#include "ppm.h"
#include "net.h"
// --- ==== --- //


// --- PPM --- //
// 16 bits per channel, big endian (I think).
void ICACHE_FLASH_ATTR net_callback_ppm(void *arg, char *data, unsigned short len)
{
	struct espconn *conn = (struct espconn *)arg;

	if (len == N_CHANNELS * sizeof(uint16_t))
	{
		ppm_reset_failsafe();

		for (int channel = 0; channel < N_CHANNELS; ++channel)
		{
			int channel_offset = channel * sizeof(uint16_t);
			uint16_t value = *(data + channel_offset) << 8 | *(data + channel_offset + 1);
			ppm_set_channel(channel, value);
		}
	}
}
// --- ==== --- //


// --- Main --- //
void ICACHE_FLASH_ATTR user_init(void)
{
#if OVERCLOCK
	system_update_cpu_freq(160);
#endif

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	ppm_init();

	wifi_setup();

	new_udp_listener(PPM_PORT, net_callback_ppm);
}
// --- ==== --- //


// --- UART --- //
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t *events) {
	if (events->sig == 0) {
		// Sig 0 is a normal receive. Get how many bytes have been received.
		uint8_t rx_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

		uint8_t rx_char;
		for (uint8_t ii=0; ii < rx_len; ii++) {
			rx_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

			if (rx_char == 's')
			{
				for (int channel = 0; channel < N_CHANNELS; ++channel)
				{
					uint16_t value = ppm_get_channel(channel);
					os_printf("%d = %d (%d us)\n",
						channel, value, UINT16_TO_CHANNEL_US(value));
				}
			}
		}

		// Clear the interrupt condition flags and re-enable the receive interrupt.
		WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
		uart_rx_intr_enable(UART0);
	}
}
// --- ==== --- //
