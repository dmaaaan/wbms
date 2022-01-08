#include <stdint.h>

#define LTC6812_422K 0U
#define LTC6812_27K   1U
#define LTC6812_7K  2U
#define LTC6812_26HZ 3U

#define LTC6812_ADC_MD  LTC6812_7K
#define LTC6812_ADC_DCP 0 //Discharge Not Permitted
#define LTC6812_NUM_ADCCH 0 //Cell selection for adc conversion- all cells
#define LTC6812_PLADC (0x0714)
