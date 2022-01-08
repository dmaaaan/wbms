#include "buildcfg.h"

#if NODETYPE == NODETYPE_BMS

#include "app_master.h"
#include "led.h"
#include "fnv.h"
#include "hg_mesh.h"
#include "hg_netMgr.h"
#include "hg_test.h"

void diag_fn(void *p1, void *p2, void *p3);
void mercury_main_bms(void *p1, void *p2, void *p3);

extern struct k_thread diag_thread;
extern struct k_thread listen_thread;

extern hgNetState NetState;
extern t_bmsSt systemStateLog[NODES_NUM];

extern struct k_timer ok_timeout_timer;
extern uint8_t timer_started_node;

void blink()
{
	mercury_led_flash(50);
	k_msleep(50);
	mercury_led_flash(50);
	k_msleep(50);
}


void trustFunc(t_rxCvPkt *rxmsg){
//empty
}

int crc16Check(t_rxCvPkt *rxmsg){
    int ret=0;
    uint16_t crc = crc16_ansi(rxmsg->data,LTC_NCELLS);
    if(crc == rxmsg->pec)
	ret = 1;
    else{
	printk("ERR: PEC FAILED for %d (token: %d): Rx: %d, Exp from data: %d\n",rxmsg->addr,rxmsg->token,rxmsg->pec, crc );
    }
    return ret;
}

void diag_checkNodePresence() //enters always forming() if one node crashes
{
    uint32_t now = k_uptime_get_32();
    uint32_t lastNodeRx;
    uint32_t diff;
    if(NetState.hm.currSys != STARTUP_NODATA)//prevent it from being entered at startup
    {
	  for (uint16_t i = 0; i < NODES_NUM; i++) 
	  {
	      lastNodeRx = NetState.client[i].diag.lastReceivedOk;
	      diff = now-lastNodeRx;
	      if(diff>CMB_MSG_TIMEOUT &&NetState.client[i].diag.isActive==1){ //during that time all the nodes turn inactive so this is not working just for one node
		  mercury_netMgr_updateNodeState(i+BASE_ADDR,INACTIVE);
		  printk("decreasing %lu\n",lastNodeRx);
		  NetState.hm.active--;
		  NetState.client[i].diag.isActive = 0; //joining timestamp of a node

	      }
	  }
	  if(NetState.hm.active==0){ //not working
	      printk("ERR: No active nodes!\n");
	      mercury_netMgr_InitState();
	      mercury_mesh_forming();
	      mercury_mesh_sendStart();
	  }
    }
}

void diag_verifyRxData(t_rxCvPkt *m)
{
      t_txTokPkt match;

      uint8_t nodeIdx= m->addr-BASE_ADDR; //node indexing should be mapped to array indexing
      t_diag * diag= &NetState.client[nodeIdx].diag;
  
      if(message_dev_searchTX(m,&match)){ // basic token check 
	  //trustFunc(&rxmsg); 
      }
      else
      {
	  mercury_netMgr_transactionUpdate(m->addr, REJECTED);
	  systemStateLog[nodeIdx].currNode = TOKEN_MISMATCH;
	  mercury_netMgr_errorCountUpdate(m->addr,TOKEN_MISMATCH);
      }

      if(crc16Check(m))
	  mercury_netMgr_transactionUpdate(m->addr,ENDED); 
      else{
	  mercury_netMgr_transactionUpdate(m->addr, REJECTED);
	  systemStateLog[nodeIdx].currNode = PEC_FAILED;
	  mercury_netMgr_errorCountUpdate(m->addr,PEC_FAILED);
      }
      NetState.client[nodeIdx].diag.lastUpdated = k_uptime_get_32(); //exported to Radon
}


void diag_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	static t_rxCvPkt m; //pointer to received message data
	printk("STARTUP: %d\n",NetState.info.session.startupTs);
	mercury_netMgr_timeoutInit();
	printk("Timeouts init\n");
	while(1){
	      //printk("Thread: diag_fn\n");
	      if(message_dev_fetchRX(&m)!=0) //if we fetched sth (return 1), process it
	      {
		    diag_verifyRxData(&m);
	      }
	      //diag_checkNodePresence(); //used but it is not working as expected. only able to get back into waiting state and reset values
	      //mercury_mesh_reforming(); //inside is a condition that will only allow to enter if system is in BATT_INFO_LOST state
	      //mercury_mesh_checkActiveNodes(); //doesnt do anything
	      k_msleep(DIAG_SLEEP_TIME);
	}
}

void queue_info(struct k_msgq * msgq)
{
      struct k_msgq_attrs attrs;
      k_msgq_get_attrs(msgq,&attrs);
      int curr_size = attrs.used_msgs;
      printk("%d msgs (of size %d [B]) out of %d used.\n",curr_size,attrs.msg_size,attrs.max_msgs);
}

void mercury_main_bms(void *p1, void *p2, void *p3) 
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *dev;
	uint32_t printInt, prevPrint;

	dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	if (dev == NULL) {
		return;
	}
	printk("Sending start\n");
	mercury_mesh_sendStart();//sends start signal for all nodes. implement clock synch in there too
	sessionInfo();
	radon_configInfoTX();

	while(1)
	{
	    printInt = k_uptime_get_32() - prevPrint;
	    //printk(">Thread: main handler\n");
	    radon_latenciesInfoTX();
	    if(printInt>=PRINT_PERIOD){
		//print_data_bucket();
		print_summary();   
		printMeshHealth();
		radon_countersInfoTX();
		prevPrint = k_uptime_get_32();
	    }
	    //printk(">Thread: mercury_main_bms()\n");
	    blink();
	    k_msleep(MAIN_SLEEP_TIME);
	}
}


void mercury_bms_init()
{
    //If BMS, initialise the network management state info
    mercury_netMgr_InitState();//NetState passed as a global extern 
}

#endif