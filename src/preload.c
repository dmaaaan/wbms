/*
 * Contains useful pre-load stuff for MErcury that runs before it jumps into the main firmware
 */
#include <zephyr.h>
#include <drivers/uart.h>
#include <power/reboot.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>

#define UART_DEVICE_NAME CONFIG_UART_CONSOLE_ON_DEV_NAME

/*
 * Disable protections that would prevent us running code from RAM
 */
#include <arch/arm/aarch32/cortex_m/cmsis.h>
void mercury_preload_disableMPU(void)
{
	uint32_t index;
	/* Kept the max index as 8(irrespective of soc) because the sram
	 * would most likely be set at index 2.
	 */
	for (index = 0U; index < 8; index++) {
		MPU->RNR = index;
		if (MPU->RASR & MPU_RASR_XN_Msk) {
			MPU->RASR ^= MPU_RASR_XN_Msk;
		}
	}
}

/*
 * Reads a line from UART, assumes a CR/LF ending (either or)
 */
void mercury_preload_RXLine(char *dest, uint8_t *rSize, uint16_t timeout)
{
	uint32_t time_entry = k_cycle_get_32();

	unsigned char cIn;
	uint8_t		  bPos = 0;

	const struct device *uart_dev = device_get_binding(UART_DEVICE_NAME);

	if (!uart_dev) {
		return;
	}

	//Wait for up to timeout ms for something to arrive on UART or for the line to be finished
	while (k_cycle_get_32() - time_entry < timeout || bPos > 0) {
		if (uart_poll_in(uart_dev, &cIn) == 0) {
			//Accept either a CR/LF line ending
			//Worst case a windows box send both and every second line is blank
			if (cIn == 10 || cIn == 13) {
				break;
			} else {
				dest[bPos] = cIn;
				bPos += 1;
			}
		}
	}

	*rSize = bPos;
}
