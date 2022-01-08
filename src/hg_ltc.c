#include "hg_ltc.h"
#include "ltc_cmd.h"
#include <random/rand32.h>

//-----------STUBS----

 hgLTC_State cmbData;

void poll_fake_cv(){  
    for(int i=0;i<LTC_NCELLS;i++){
	  sys_rand_get(&cmbData.cellVolts[i],8*CV_SIZE); //8 bit value
	  //printk("LTC: %d, \n",cmbData.cellVolts[i]);
    } 
    //printk("\n");
}

//-------------------

void ltc_startadc();
void ltc_convadc();
void ltc_readcv(hgLTC_State *state);

uint16_t mercury_ltc_calcPEC(uint8_t  len, //Number of bytes that will be used to calculate a PEC
							 uint8_t *data //Array of data that will be used to calculate  a PEC
)
{
	uint16_t remainder, addr;
	remainder = 16; //initialize the PEC

	for (uint8_t i = 0; i < len; i++) // loops for each byte in data array
	{
		addr	  = ((remainder >> 7) ^ data[i]) & 0xff; //calculate PEC table address
		remainder = (remainder << 8) ^ crc15Table[addr];
	}

	return (remainder * 2); //The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}

void mercury_ltc_csLow()
{
	gpio_pin_set(LTC_CS_PIN_DEV, LTC_CS_PIN, 0);
}

void mercury_ltc_csHigh()
{
	gpio_pin_set(LTC_CS_PIN_DEV, LTC_CS_PIN, 1);
}

void mercury_ltc_wakeIdle()
{
	mercury_ltc_csLow();

	{
		uint8_t wData, rData;

		wData = 0xff;
		mercury_spi_read_seq(&wData, 1, &rData, 1);
	}

	mercury_ltc_csHigh();
}

void mercury_ltc_wakeSleep()
{
	mercury_ltc_csLow();
	k_usleep(300);
	mercury_ltc_csHigh();
	k_usleep(10);
}
/*!**********************************************************************
 \brief  Initializes hardware and variables
  @return void
 ***********************************************************************/
int16_t mercury_ltc_init()
{
	//Set up CS pin for output
	gpio_pin_configure(LTC_CS_PIN_DEV, LTC_CS_PIN, GPIO_OUTPUT);
	gpio_pin_set(LTC_CS_PIN_DEV, LTC_CS_PIN, 1);

	mercury_ltc_wakeSleep();

	//Send init command streams. ref p.66 of ltc 6812 spec
	{
		//Set config bits CFGRA to: REFON,ADCOPT,GPIOBITS_A,DCCBITS_A, DCTOBITS, UV, OV, PEC0, PEC1
		uint8_t CMDStream_WCfgA[12] = { 0x0, 0x1, 0x3D, 0x6E, 0xE4, 0x52, 0x27, 0xA0, 0x0, 0x50, 0x47, 0x48 };
		// Set config bits CFGRB to: FDRF,DTMEN,PSBits,GPIOBITS_B,DCCBITS_B, PEC0, PEC1
		uint8_t CMDStream_WCfgB[12] = { 0x0, 0x24, 0xB1, 0x9E, 0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x90, 0x5A };
		//write CMDStream_WCfgA on spi
		mercury_ltc_csLow();
		mercury_spi_write(CMDStream_WCfgA, sizeof(CMDStream_WCfgA));
		mercury_ltc_csHigh();
		//write CMDStream_WCfgB on spi
		mercury_ltc_csLow();
		mercury_spi_write(CMDStream_WCfgB, sizeof(CMDStream_WCfgB));
		mercury_ltc_csHigh();
	}

	mercury_ltc_wakeIdle();

	//Read back the config data
	uint8_t CMDStream_RCfgA[4] = { 0x0, 0x2, 0x2B, 0xA };
	uint8_t CMDBuffer_RCfgA[8];

	uint8_t CMDStream_RCfgB[4] = { 0x0, 0x26, 0x2C, 0xC8 };
	uint8_t CMDBuffer_RCfgB[8];

	mercury_ltc_csLow();
	mercury_spi_read_seq(CMDStream_RCfgA, sizeof(CMDStream_RCfgA), CMDBuffer_RCfgA, 8);
	mercury_ltc_csHigh();
	mercury_ltc_csLow();
	mercury_spi_read_seq(CMDStream_RCfgB, sizeof(CMDStream_RCfgB), CMDBuffer_RCfgB, 8);
	mercury_ltc_csHigh();
	//Calculate PEC of returned data
	uint16_t pecA = mercury_ltc_calcPEC(6, CMDBuffer_RCfgA);
	uint16_t pecB = mercury_ltc_calcPEC(6, CMDBuffer_RCfgB);

	uint16_t pecARef = (0x47 << 8) | 0x48;
	uint16_t pecBRef = (0x90 << 8) | 0x5A;

	int8_t retCode = 0;

	if (pecA != pecARef) {
		retCode -= 1;
	}

	if (pecB != pecBRef) {
		retCode -= 10;
	}


	return retCode;

	/*
	S0xE4, 0x52, 0x27, 0xA0, 0x00, 0x50}
	0x00, 0x08, 0x00, 0x00, 0x00, 0x00}
     */
}


int16_t mercury_ltc_updateState(hgLTC_State *state)
{
	if (state == NULL) {
		return -1;
	}

		ltc_convadc();//poll adc- method 2 ref ltc spec
		ltc_readcv(state);//Read the voltage data


	return 0;
}

/* Starts ADC conversion for cell voltage */
void ltc_startadc(){
  uint8_t md_bits;
  uint8_t cmd[4];//conversion command
  uint16_t cmd_pec;

  //see p. 60 & 61 from ltc6812 spec
  md_bits = (LTC6812_ADC_MD & 0x02) >> 1;
  cmd[0] = md_bits + 0x02;
  md_bits = (LTC6812_ADC_MD & 0x01) << 7;
  cmd[1] =  md_bits + 0x60 + (LTC6812_ADC_DCP<<4) + LTC6812_NUM_ADCCH;
  //cmd should be 0x360 for current setting !!

  cmd_pec = mercury_ltc_calcPEC(2, cmd); //should take only first two bytes. for this config: cmd- 0x0360
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);
  //printk("startadc; %x%x\n",cmd[0],cmd[1]);

  mercury_ltc_wakeSleep();

  //Tell Ltc to start sampling the cell voltages
  mercury_ltc_csLow();
  mercury_spi_write(cmd, 4);
  mercury_ltc_csHigh();
}



// Start Cell ADC Measurement and wait for finish
/* This function will block operation until the ADC has finished it's conversion- see for loop*/
void ltc_convadc(){

    uint32_t iter	   = 0;
    uint32_t	 convTime  = 0;
    uint8_t	 currentTimeByteTx = 0xff;
    uint8_t	 currentTimeByteRx = 0;
    bool	 pollOkay  = false;
    uint8_t cmd[4];
    uint16_t cmd_pec;

    //init and start conversion adc
  ltc_startadc();

    cmd[0] |= (LTC6812_PLADC>> 8); //0x07
    cmd[1] |= (LTC6812_PLADC);  //0x14

    cmd_pec = mercury_ltc_calcPEC(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec);

    //printk("convadc; %x%x\n",cmd[0],cmd[1]);

		//Poll the ADC for done status
		mercury_spi_write(cmd, 4);
		//hold CSB low after an ADC conversion command is sent.
		mercury_ltc_csLow();

		/*After entering the PLADC command, SDO will go low if
the device is busy performing conversions.*/
		while (iter < 20000) {
      //reading means writing 0xFF on the SDO to pull down this line
			mercury_spi_read_seq(&currentTimeByteTx, 1, &currentTimeByteRx, 1);
      //end condition- returns positive time value on completion of adc conversion- see polling method 2
			if (currentTimeByteRx > 0) {
				pollOkay = true;
				break;
			} else {
				iter += 1;
			}
		}
		mercury_ltc_csHigh();

		// if (!pollOkay) {
		// 	return -2;
		// }
    convTime = iter * 10; //add it to struct for the measurement
}

//Read Cell Voltage Registers
void ltc_readcv(hgLTC_State *state){

	uint8_t CMDBuffer_ReadVR[8];
  uint8_t cellN = 0;
	uint8_t mPos;

	uint8_t CMDStream_ReadVR[5][4] = {
	            { 0x0, 0x4, 0x7, 0xC2 },
	            { 0x0, 0x6, 0x9A, 0x94 },
	            { 0x0, 0x8, 0x5E, 0x52 },
	            { 0x0, 0xA, 0xC3, 0x4 },
	            { 0x0, 0x9, 0xD5, 0x60 } };

	mercury_ltc_wakeSleep();

	for (int16_t i = 0; i < 5; i++) {
  		mercury_ltc_csLow();
  		mercury_spi_read_seq(CMDStream_ReadVR[i], 4, CMDBuffer_ReadVR, 8);
  		mercury_ltc_csHigh();
			mPos = 0;
  		for (int16_t j = 0; j < 3; j++) {
    			state->cellVolts[cellN] = CMDBuffer_ReadVR[mPos] + (CMDBuffer_ReadVR[mPos + 1] << 8);
    			cellN += 1;
    			mPos += 2;
		}
	}

}
