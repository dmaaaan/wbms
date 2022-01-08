#include "hg_spi.h"

void mercury_spi_setup(uint8_t pin_MISO, uint8_t pin_MOSI, uint8_t pin_SCK, uint32_t speed)
{
	const struct device *dev;

	dev = device_get_binding("GPIO_0");
	gpio_pin_configure(dev, pin_MOSI, GPIO_OUTPUT);
	gpio_pin_configure(dev, pin_SCK, GPIO_OUTPUT);

	//Define pins and speed mode for SPI buf
	reg_SPIM0_PSelMISO	= (0 << 31) | pin_MISO;
	reg_SPIM0_PSelMOSI	= (0 << 31) | pin_MOSI;
	reg_SPIM0_PSelSCK	= (0 << 31) | pin_SCK;
	reg_SPIM0_Frequency = speed;

	//Disable easyDMA list mode
	reg_SPIM0_RXDList = 0;
	reg_SPIM0_TXDList = 0;

	//Set over-reach character
	reg_SPIM0_ORC = 0xFF;

	//Default config (MSB first, Leading edge SCK phase, SCK active high)
	reg_SPIM0_Config = 0;

	reg_SPIM0_Enable = 7;
}

void mercury_spi_write(uint8_t *src, uint8_t len)
{
	reg_SPIM0_TXDMaxLen = len;
	reg_SPIM0_TXDPtr	= (uint32_t)src;

	reg_SPIM0_EventsEnd = 0;
	reg_SPIM0_TaskStart = 1;

	while (reg_SPIM0_EventsEnd == 0) {
	}
}

void mercury_spi_read(uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len)
{
	reg_SPIM0_RXDPtr	= (uint32_t)rx_data;
	reg_SPIM0_RXDMaxLen = rx_len;

	reg_SPIM0_TXDPtr	= (uint32_t)tx_data;
	reg_SPIM0_TXDMaxLen = tx_len;

	reg_SPIM0_EventsEnd = 0;
	reg_SPIM0_TaskStart = 1;

	while (reg_SPIM0_EventsEnd == 0) {
	}
}

void mercury_spi_read_seq(uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len)
{
	uint8_t RXBuffer[tx_len + rx_len];

	reg_SPIM0_RXDPtr	= (uint32_t)RXBuffer;
	reg_SPIM0_RXDMaxLen = rx_len + tx_len;

	reg_SPIM0_TXDPtr	= (uint32_t)tx_data;
	reg_SPIM0_TXDMaxLen = tx_len;

	reg_SPIM0_EventsEnd = 0;
	reg_SPIM0_TaskStart = 1;

	while (reg_SPIM0_EventsEnd == 0) {
	}

	for (uint16_t i = 0; i < rx_len; i++) {
		rx_data[i] = RXBuffer[i + tx_len];
	}
}

uint8_t mercury_read_byte(uint8_t tx_data)
{
	uint8_t data;

	mercury_spi_read(&tx_data, 1, &data, 1);

	return data;
}

void mercury_spi_write_test()
{
	uint8_t buffB[20];

	{
		uint8_t buffA[20] = { 0, 1, 61, 110, 228, 82, 39, 160, 0, 80, 71, 72, 228, 82, 39, 160, 0, 80, 71, 72 };

		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 20, buffB, 0);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}

	{
		uint8_t buffA[20] = { 0, 36, 177, 158, 0, 8, 0, 0, 0, 0, 144, 90, 0, 8, 0, 0, 0, 0, 144, 90 };
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 20, buffB, 0);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}

	{
		uint8_t buffA[1] = { 0xFF };
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 1, buffB, 1);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}

	{
		uint8_t buffA[1] = { 0xFF };
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 1, buffB, 1);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}

	{
		uint8_t buffA[4] = { 0, 2, 43, 10 };
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 4, buffB, 16);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}

	{
		uint8_t buffA[4] = { 0, 38, 44, 200 };
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 0);
		mercury_spi_read(buffA, 4, buffB, 16);
		gpio_pin_set(device_get_binding("GPIO_0"), 3, 1);
	}
}
