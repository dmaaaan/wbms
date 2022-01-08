//Headers for the bootloader funzies
#include <string.h>
#include <sys/printk.h>
#include <stdlib.h>
#include <drivers/uart.h>
#include <power/reboot.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>

#include "led.h"

#define UART_DEVICE_NAME  CONFIG_UART_CONSOLE_ON_DEV_NAME

#define FWUPD_STAGING_POS 0x40000
#define FWUPD_SIZE_POS	  0x7FFFC

/*
 * Final stage of the update process, copy the update file back to main memory
 */

#define cfgRegister	  *((volatile uint32_t *)(0x4001E000 + 0x504))
#define eraseRegister *((volatile uint32_t *)(0x4001E000 + 0x508))
#define readyRegister *((volatile uint32_t *)(0x4001E000 + 0x400))

#define aircrRegister *((volatile uint32_t *)(0xE000EDC))

void mercury_fwupd_stageF()
{
	mercury_led_done(2);

	//Erase the flash to allow for writing of the downloaded update
	uint32_t baseAddr = 0;
	for (uint32_t z = 0; z < 64; z++) {
		cfgRegister = 2;
		while (readyRegister == 0) {
		}

		eraseRegister = baseAddr;
		baseAddr += 0x1000;
		while (readyRegister == 0) {
		}
	}

	volatile uint32_t *sourceAddr = (uint32_t *)FWUPD_STAGING_POS;
	volatile uint32_t *destAddr	  = (uint32_t *)0;
	uint32_t		   pos		  = 0x0000;
	while (pos < 0x3FFFF) {
		cfgRegister = 1;
		while (readyRegister == 0) {
		}

		*destAddr = *sourceAddr;

		while (readyRegister == 0) {
		}

		destAddr += 1;
		sourceAddr += 1;
	}

	//TODO: This should reset the chip but doesnt. Needs investigated.
	aircrRegister = (0x5FA << 16) | (1 << 2);
}

void mercury_fwupd_entryPoint()
{
	//obtain flash device to write to
	const struct device *flash_dev = device_get_binding("NRF_FLASH_DRV_NAME");

	if (!flash_dev) {
		mercury_led_fatal(3);

		return;
	}

	const struct device *uart_dev = device_get_binding(UART_DEVICE_NAME);

	if (!uart_dev) {
		mercury_led_fatal(4);

		return;
	}

	printk("ready\n");

	mercury_led_done(1);

	const uint8_t maxBSize = 240;
	uint8_t		  bufData[maxBSize], block2[maxBSize];
	int16_t		  buffPos	  = 0;
	int8_t		  payloadSize = 0;
	uint8_t		  mType		  = 0;
	uint8_t		  cIn;
	uint32_t	  baseAddr = 0;

	//Erase the memory where the update firmware will be stored
	{
		struct flash_pages_info info;

		for (int z = 64; z < 128; z++) {
			flash_get_page_info_by_idx(flash_dev, z, &info);

			if (flash_erase(flash_dev, info.start_offset, info.size) != 0) {
				mercury_led_fatal(6);

				return;
			}
		}
	}

	bool ledState = false;
	//Download the firmware to FWUPD_STAGING_POS from mflash
	while (true) {
		//Receive message type
		while (uart_poll_in(uart_dev, &mType) != 0) {
		}
		uart_poll_out(uart_dev, mType);

		//All data payloads are type 0, anything else and we are done with data
		if (mType != 0) {
			break;
		}

		//Receive base address
		buffPos = 0;
		while (buffPos < 4) {
			if (uart_poll_in(uart_dev, &cIn) == 0) {
				bufData[buffPos] = cIn;
				buffPos += 1;
				uart_poll_out(uart_dev, cIn);
			}
		}

		baseAddr = bufData[0] + (256 * bufData[1]) + (256 * 256 * bufData[2]) + (256 * 256 * 256 * bufData[3]);

		//Receive payload size
		while (uart_poll_in(uart_dev, &payloadSize) != 0) {
		}
		uart_poll_out(uart_dev, payloadSize);

		buffPos = 0;
		while (buffPos < payloadSize) {
			if (uart_poll_in(uart_dev, &cIn) == 0) {
				bufData[buffPos] = cIn;

				buffPos += 1;
				uart_poll_out(uart_dev, cIn);
			}
		}

		flash_write(flash_dev, FWUPD_STAGING_POS + baseAddr, bufData, payloadSize);
		flash_read(flash_dev, FWUPD_STAGING_POS + baseAddr, block2, payloadSize);

		bool egal = true;
		for (int z = 0; z < payloadSize; z++) {
			if (block2[z] != bufData[z]) {
				egal = false;

				//If this executes we have a problem with corrupted flash, go into failure mode 3
				mercury_led_fatal(3);
			}
		}

		//printk("%x s=%d [%2X %2X %2X %2X %2X %2X %2X %2X] %d \n",baseAddr+FWUPD_STAGING_POS, payloadSize, bufData[0],bufData[1],bufData[2],bufData[3],bufData[4],bufData[5],bufData[6],bufData[7], egal);

		//Flash LED for 1ms on each block to indicate things are happening
		ledState = !ledState;
		mercury_led_setState(ledState);
	}

	mercury_led_done(2);
	mercury_fwupd_stageF();
}
