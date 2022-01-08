#include <stdint.h>
#include <device.h>
#include <drivers/gpio.h>

#define reg_SPIM0_Base			0x40003000

#define reg_SPIM0_TaskStart		*((volatile uint32_t *)(reg_SPIM0_Base + 0x010))
#define reg_SPIM0_TaskStop		*((volatile uint32_t *)(reg_SPIM0_Base + 0x014))
#define reg_SPIM0_TaskSuspend	*((volatile uint32_t *)(reg_SPIM0_Base + 0x01C))
#define reg_SPIM0_TaskResume	*((volatile uint32_t *)(reg_SPIM0_Base + 0x020))

#define reg_SPIM0_EventsStopped *((volatile uint32_t *)(reg_SPIM0_Base + 0x104))
#define reg_SPIM0_EventsEndRX	*((volatile uint32_t *)(reg_SPIM0_Base + 0x110))
#define reg_SPIM0_EventsEnd		*((volatile uint32_t *)(reg_SPIM0_Base + 0x118))
#define reg_SPIM0_EventsEndTX	*((volatile uint32_t *)(reg_SPIM0_Base + 0x120))
#define reg_SPIM0_EventsStarted *((volatile uint32_t *)(reg_SPIM0_Base + 0x14C))

#define reg_SPIM0_Shorts		*((volatile uint32_t *)(reg_SPIM0_Base + 0x200))

#define reg_SPIM0_IntenSet		*((volatile uint32_t *)(reg_SPIM0_Base + 0x304))
#define reg_SPIM0_IntenClear	*((volatile uint32_t *)(reg_SPIM0_Base + 0x308))

#define reg_SPIM0_Enable		*((volatile uint32_t *)(reg_SPIM0_Base + 0x500))

#define reg_SPIM0_PSelSCK		*((volatile uint32_t *)(reg_SPIM0_Base + 0x508))
#define reg_SPIM0_PSelMOSI		*((volatile uint32_t *)(reg_SPIM0_Base + 0x50C))
#define reg_SPIM0_PSelMISO		*((volatile uint32_t *)(reg_SPIM0_Base + 0x510))
#define reg_SPIM0_Frequency		*((volatile uint32_t *)(reg_SPIM0_Base + 0x524))

#define reg_SPIM0_RXDPtr		*((volatile uint32_t *)(reg_SPIM0_Base + 0x534))
#define reg_SPIM0_RXDMaxLen		*((volatile uint32_t *)(reg_SPIM0_Base + 0x538))
#define reg_SPIM0_RXDAmount		*((volatile uint32_t *)(reg_SPIM0_Base + 0x53C))
#define reg_SPIM0_RXDList		*((volatile uint32_t *)(reg_SPIM0_Base + 0x540))

#define reg_SPIM0_TXDPtr		*((volatile uint32_t *)(reg_SPIM0_Base + 0x544))
#define reg_SPIM0_TXDMaxLen		*((volatile uint32_t *)(reg_SPIM0_Base + 0x548))
#define reg_SPIM0_TXDAmount		*((volatile uint32_t *)(reg_SPIM0_Base + 0x54C))
#define reg_SPIM0_TXDList		*((volatile uint32_t *)(reg_SPIM0_Base + 0x550))

#define reg_SPIM0_Config		*((volatile uint32_t *)(reg_SPIM0_Base + 0x554))
#define reg_SPIM0_ORC			*((volatile uint32_t *)(reg_SPIM0_Base + 0x5C0))

#define SPI_Speed_125K			0x02000000
#define SPI_Speed_250K			0x04000000
#define SPI_Speed_500K			0x08000000
#define SPI_Speed_1M			0x10000000
#define SPI_Speed_2M			0x20000000
#define SPI_Speed_4M			0x40000000
#define SPI_Speed_8M			0x80000000

void mercury_spi_setup(uint8_t pin_MISO, uint8_t pin_MOSI, uint8_t pin_SCK, uint32_t speed);
void mercury_spi_write(uint8_t *src, uint8_t size);
void mercury_spi_read(uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len);
void mercury_spi_read_seq(uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len);
void mercury_spi_write_test();

uint8_t mercury_read_byte(uint8_t tx_data);
