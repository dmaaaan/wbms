#include "buildcfg.h"

#if NODETYPE == NODETYPE_CMB

#include "app_slave.h"
#include "hg_mesh.h"
#include "hg_netMgr.h"

#include "hg_spi.h"
#include "hg_ltc.h"

hgNetClient NetCli;
struct k_sem startSem;
struct k_sem ack;
uint8_t okSgn;
uint8_t rcvSgn;
uint8_t restartSgn;
extern t_logCmb cmbInfo;
extern hgLTC_State cmbData;
t_tx newPkt; 
uint16_t currCrc;

void mercury_main_cmb(void *p1, void *p2, void *p3);
void polling_fn(void *p1, void *p2, void *p3);

K_MUTEX_DEFINE(pktMutex);
K_SEM_DEFINE(pollSem,1,1);

void blink()
{
	mercury_led_flash(50);
	k_msleep(50);
	mercury_led_flash(50);
	k_msleep(50);
}

void mercury_wakeup_main_cmb(){

}

void calcCrc(){
    currCrc = crc16_ansi(cmbData.cellVolts,LTC_NCELLS);
    //printk("PEC: %d\n", currCrc);
}

void mercury_acquire_nodeAddress(){
      bool flag_done = false;
      //Wait for the BMS to give us an address on the network
      while (mercury_mesh_getMeshAddr() == NODE_CMB_DEFADDR) {
	      mercury_mesh_requestAddress();
	      //printk("Sent addr rq\n");
	
	      uint32_t t0 = k_uptime_get_32();
	
	      while (k_uptime_get_32() - t0 < 2000) { //waits 2sec for new address
		      if (mercury_mesh_getMeshAddr() != NODE_CMB_DEFADDR) {
			      flag_done = true;
			      break;
		      }
		
		      k_msleep(10);
	      }
	
	      if (flag_done) {
		      break;
	      }
      }
      printk("Finished aquiring an address. CMB is now at addr=%d\n", mercury_mesh_getMeshAddr());
}

void mercury_cmb_init()
{
//CMBs need a working SPI bus for talking to the LTC
      mercury_spi_setup(14, 13, 12, SPI_Speed_250K);
      k_msleep(500);

      //Bring up the LTC
      int8_t LTCInitCode = mercury_ltc_init();
	if (LTCInitCode != 0) {
	      printk("Error initialising LTC code=%d!\n", LTCInitCode);
      }
}


void polling_fn(void *p1, void *p2, void *p3) {

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while(1){
	    //printk(">Thread: polling()\n");
	  k_sem_take(&pollSem,K_FOREVER);
	  poll_fake_cv();
	  calcCrc();
	  k_sem_give(&pollSem);
	  blink(); 
	  k_msleep(POLLING_INT);
	}
}

void mercury_main_cmb(void *p1, void *p2, void *p3) 
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_init(&startSem, 0, 1);
	k_sem_init(&ack, 0, 1);
	k_sem_take(&startSem,K_FOREVER);
	printk("Starting main..\n");
	mercury_netMgr_sendOK(NODE_BMS_ADDR);
	okSgn=0;
	rcvSgn=0;
	restartSgn =0;
	while(1){
		//printk(">Thread: mercury_main_cmb()\n");
		if(okSgn || rcvSgn || restartSgn){//if ACK or RECOVERY was received
		    mercury_netMgr_sendOK(NODE_BMS_ADDR);
		    okSgn= 0;
		    rcvSgn=0;
		    restartSgn=0; //if restart was received
		}
		k_msleep(OK_STATUS_INTERVAL_MS);
	}
}

#endif
