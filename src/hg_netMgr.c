#include "hg_netMgr.h"
#include "led.h"
#include "hg_mesh.h"
#include "hg_test.h"

#if NODETYPE == NODETYPE_BMS

K_MSGQ_DEFINE(rx_msgq, sizeof(t_rxCvPkt) , SIZE_OF_RXQUEUE, 4);
K_MSGQ_DEFINE(tx_msgq, sizeof(t_txTokPkt) , SIZE_OF_TXQUEUE, 4);
K_FIFO_DEFINE(hil_txqueue);
struct k_timer ok_timeout[NODES_NUM];
struct k_timer data_timeout[NODES_NUM];
uint8_t timer_nodeIdx[NODES_NUM];



#elif NODETYPE == NODETYPE_CMB

K_MSGQ_DEFINE(rx_msgq, sizeof(t_rx) , SIZE_OF_RXQUEUE, 4);
K_MSGQ_DEFINE(tx_msgq, sizeof(t_tx) , SIZE_OF_TXQUEUE, 4);

#endif

#if NODETYPE == NODETYPE_BMS

void printMeshHealth();

char * timings_pktPtr;//used by message_dev_pushRadonQ()


hgNetState NetState;
t_transCnt transLog[NODES_NUM];
t_pktCnt  pktCntLog[NODES_NUM];
/*keeps track of all states*/
t_bmsSt systemStateLog[NODES_NUM];


#elif NODETYPE == NODETYPE_CMB

#endif

#if NODETYPE == NODETYPE_BMS

/*part of diagnostic thread. tries to reestablish the connection if node disappeared*/
void mercury_mesh_checkActiveNodes(){// not used
     if(NetState.hm.active!=NODES_NUM){
	printk("ERR: Degraded state!\n");

     }
}
///*only entered if restart was triggered*/
void mercury_mesh_reforming() //not used
{
    if(NetState.hm.currSys == BATT_INFO_LOST){ //only use the function if the system has to be restarted
          while(NetState.hm.active!=NODES_NUM){
	      printk("WRN: RESTART: Waiting for all nodes to join..\n");
	      k_msleep(500);
	  }
	  NetState.info.session.startupTs = k_uptime_get_32(); //this is time reference for system time measurements in BMS
	  printMeshHealth();   
    }
}

/*used in main.c to form full network*/
void mercury_mesh_forming()
{
      while(NetState.hm.active!=NODES_NUM){
	  printk("WRN: Waiting for all nodes to join..\n");
	  k_msleep(500);
      }
      NetState.info.session.startupTs = k_uptime_get_32(); //this is time reference for system time measurements in BMS
      printMeshHealth();
      printConfigSt();
}

 /*started when we send token and expect data*/
void mercury_netMgr_startDataTimer(uint8_t nodeIdx){
    k_timer_start(&data_timeout[nodeIdx], K_MSEC(DATA_TIMER_VALUE), K_NO_WAIT);
}

 /*stopped when data msg was received*/
void mercury_netMgr_stopDataTimer(uint8_t nodeAddr){
    uint8_t nodeIdx= nodeAddr -BASE_ADDR;
    uint32_t expiryt = k_timer_remaining_get(&data_timeout[nodeIdx]);
    if(expiryt>0){
	k_timer_stop(&data_timeout[nodeIdx]);
	printk("DATA: Timer for node %d stopped %d ms before expiry\n",nodeAddr, expiryt);
    } 
}

 /*started when we send ack and expect ok*/
void mercury_netMgr_startOkTimer(uint8_t nodeIdx){
    k_timer_start(&ok_timeout[nodeIdx], K_MSEC(OK_TIMER_VALUE), K_NO_WAIT);
}

/*stopped when ok was received*/
void mercury_netMgr_stopOkTimer(uint8_t nodeAddr){ 
    uint8_t nodeIdx= nodeAddr -BASE_ADDR;
    uint32_t expiryt = k_timer_remaining_get(&ok_timeout[nodeIdx]);
    if(expiryt>0){
	k_timer_stop(&ok_timeout[nodeIdx]);
	printk("OK: Timer for node %d stopped %d ms before expiry\n",nodeAddr, expiryt);
    } 
}

void flushFifos(){

}

/*if no data was received from a node then restart the whole process with all the nodes by sending start signal to all of them*/
void data_expiry_function(struct k_timer *timer_id){
    uint8_t *nodeAddr;
    nodeAddr= k_timer_user_data_get(timer_id);
    printk("ERR: Timeout expired for %d\n", *nodeAddr);
    uint8_t lostNode = *nodeAddr;
    mercury_netMgr_updateNodeState(lostNode, TIMEOUT_ERR);
    mercury_netMgr_transactionUpdate(lostNode,REJECTED);
    mercury_netMgr_updateSystemState(BATT_INFO_LOST); // this will make the system enter mesh reforming() to wait again for all the nodes to rejoin
    //mercury_mesh_resendACK(nodeIdx);
    printk(">>SEND RESTART\n");
    updateRecoverInfo(lostNode);
    mercury_netMgr_errorCountUpdate(lostNode,TIMEOUT_ERR);
    bt_mesh_cli_flushFifos(); //testing that. might cause loss of data. hard reset
    mercury_mesh_sendStart();//send start signal to all the nodes to restart the whole communication
    //mercury_mesh_resendToken(lostNode); // alternative to above. we simply resend the token


}

/*if no ack was received from a node on time then send recovery message to that node*/
void ok_expiry_function(struct k_timer *timer_id){
    uint8_t *nodeAddr;
    nodeAddr= k_timer_user_data_get(timer_id);
    printk("ERR: Timeout expired for %d\n", *nodeAddr);
    uint8_t lostNode = *nodeAddr;
    mercury_netMgr_updateNodeState(lostNode, TIMEOUT_ERR);
    mercury_netMgr_transactionUpdate(lostNode,REJECTED);
    //mercury_netMgr_updateSystemState(BATT_INFO_LOST);
    //mercury_mesh_resendACK(nodeIdx);
    printk(">>SEND RECOVERY\n");
    mercury_mesh_recoverNodes(lostNode-BASE_ADDR); 
    mercury_netMgr_errorCountUpdate(lostNode,TIMEOUT_ERR);
    updateRecoverInfo(lostNode);
}


void mercury_netMgr_timeoutInit(){
    for(int i=0;i<NODES_NUM;i++){
	k_timer_init(&ok_timeout[i], ok_expiry_function, NULL);
	k_timer_init(&data_timeout[i], data_expiry_function, NULL);
	k_timer_user_data_set(&ok_timeout[i], &NetState.client[i].addr);
	k_timer_user_data_set(&data_timeout[i], &NetState.client[i].addr);
    }

}


uint8_t mercury_netMgr_statesUpdate(uint8_t nodeAddr, t_pktS curr) //return 1 if states are mixed up
{
      uint8_t nodeIdx = nodeAddr-BASE_ADDR;
      t_bmsSt * state = &systemStateLog[nodeIdx];
      t_nodeS newNdState = state->currNode;
      state->prevPkt = systemStateLog[nodeIdx].currPkt; //put current state as previous
      t_pktS prev = state->prevPkt;
      switch(curr){
      case RX_OK:
	  if((prev != TX_ACK) && (prev != NO_INFO) && (prev != TX_RECOVER)) {
	      //printk("ERR: [OK] Unexpected previous pkt status (%s) for node %d\n",PKT_STATUS_STRING[prev], nodeAddr);
	      state->prevPkt = TX_ACK; //manually correcting the previous state to avoid propagation of this issue on the whole net
	      newNdState = PKT_ST_ERR; 
	      return 1;
	  }
	  else{
	      newNdState = ACTIVE;
	  }
	  break;
      case TX_TOKEN:
          if(prev != RX_OK)
	  { 
	      //printk("ERR: [Token] Unexpected previous pkt status (%s) for node %d\n",PKT_STATUS_STRING[prev], nodeAddr);
	      state->prevPkt = RX_OK;
	      newNdState = PKT_ST_ERR;
	      return 1;
	  }
	  break;
      case RX_DATA:
	  if(prev != TX_TOKEN) //previous state
	  {
		//printk("ERR: [Data] Unexpected previous pkt status (%s) for node %d\n",PKT_STATUS_STRING[prev], nodeAddr);
		state->prevPkt = TX_TOKEN;
		newNdState = PKT_ST_ERR; 
		return 1;
	  }
	  break;
      case TX_ACK:
	  if(prev != RX_DATA) //previous state
	  {
		//printk("ERR: [ACK] Unexpected previous pkt status (%s) for node %d\n",PKT_STATUS_STRING[prev], nodeAddr);
		state->prevPkt = RX_DATA;
		newNdState = PKT_ST_ERR; 
		return 1;
	  }
	  break;
      case TX_RECOVER:
	  printk("WRN: Node %d in TX_RECOVER state\n",nodeAddr);
	  newNdState = TIMEOUT_ERR; 
	  break;
      default:
	  break;
      }
      //printMeshHealth();
      state->currPkt = curr; //set new state as current state
      //printk("prev: %s, curr: %s\n", PKT_STATUS_STRING[state->prevPkt], PKT_STATUS_STRING[state->currPkt]);

      state->prevNode = state->currNode;
      state->currNode = newNdState;

      return 0; //no issue or recovery so dont care
}

void mercury_netMgr_transactionUpdate(uint8_t nodeAddr,t_transS type){
      uint8_t nodeIdx = nodeAddr-BASE_ADDR;
      switch(type){
      case REJECTED:
	  transLog[nodeIdx].tRejected++;
	  break;
      case STARTED:
	  transLog[nodeIdx].tStarted++;
	  break;
      case ENDED:
	  transLog[nodeIdx].tClosed++;
	  break;
      }
}

void mercury_netMgr_errorCountUpdate(uint8_t nodeAddr,t_nodeS type){
      uint8_t nodeIdx = nodeAddr-BASE_ADDR;
      t_diag * diag= &NetState.client[nodeIdx].diag;

      switch(type){
      case TOKEN_MISMATCH:
	  NetState.hm.log.tokenMisCnt++; //for the system
	  diag->log.tokenMisCnt++; //for each node separately   
	  break;
      case PEC_FAILED:
	  diag->log.pecCnt++; //separately for each node
	  NetState.hm.log.pecCnt++;
	  break;
      case TIMEOUT_ERR:
	  NetState.hm.log.timeoutCnt++; //for the system
	  diag->log.timeoutCnt++; //for each node separately 
	  break;
      case PKT_ST_ERR:
	  NetState.hm.log.droppedCnt++; //for the system
	  diag->log.droppedCnt++; //for each node separately 
	  break;
      default: //if we go a message of any type that shouldnt have come at that moment
	   break;
      }
}

void mercury_netMgr_packetCountUpdate(uint8_t nodeAddr,t_pktS type){
      uint8_t nodeIdx = nodeAddr-BASE_ADDR;
      switch(type){
      case RX_OK:
	  pktCntLog[nodeIdx].OkCnt++;
	  break;
      case TX_TOKEN:
	  pktCntLog[nodeIdx].TokCnt++;
	  break;
      case RX_DATA:
	  pktCntLog[nodeIdx].CvCnt++;
	  break;
      case TX_ACK:
	  pktCntLog[nodeIdx].AckCnt++;
	  break;
      case TX_RECOVER:
	  pktCntLog[nodeIdx].RcvCnt++; //recovery msgs
	  break;
      default: //if we go a message of any type that shouldnt have come at that moment
	   break;
      }
}


void mercury_netMgr_InitState() 
{
	hgNetClient * cli;
	NetState.hm.currSys = STARTUP_NODATA;
	for (uint16_t i = 0; i < HG_NET_MAXCLIENTS; i++) {
		cli = &NetState.client[i];
		cli->addr = 0;
		systemStateLog[i].currNode = INACTIVE;
		systemStateLog[i].currPkt = NO_INFO;
		NetState.client[i].diag.isActive = 0;
	}
	/*init state of the system is startup until first data is received*/
}

void mercury_netMgr_updateNodeState(uint16_t addr, t_nodeS state){
      uint8_t nodeIdx = addr-BASE_ADDR;
      systemStateLog[nodeIdx].prevNode = systemStateLog[nodeIdx].currNode;
      systemStateLog[nodeIdx].currNode = state;
}

void mercury_netMgr_updateSystemState(t_sysSS state){
      NetState.hm.prevSys = NetState.hm.currSys;
      NetState.hm.currSys= state;
}

/*each node acquiring and address will cause bms to enter here*/
hgNetClient* mercury_netMgr_NewClient() 
{
	int16_t nCli = -1;
	//Find an unused client to allocate
	for (uint16_t i = 0; i < HG_NET_MAXCLIENTS; i++) {
		if (systemStateLog[i].currNode == INACTIVE) { //if previous status was INACTIVE (all fields are initiated to INACTIVE then occupy it)
			printk("Found free slot at %d\n",i);
			mercury_netMgr_updateNodeState(i+BASE_ADDR,PAIRED);/*node goes into PAIRED mode at this address stored in BMS*/
			nCli = i;
			break; /*whole loop will end on first found INACTIVE slot*/
		}
	}	
	if (nCli < 0) {
		printk("FATAL: Unable to allocate new net client!\n");
		mercury_led_fatal(12);
	}
	//Find the next usable address
	uint16_t currAddr = BASE_ADDR; //new addresses start from 16 for CMBs
	for (uint16_t i = 0; i < HG_NET_MAXCLIENTS; i++) {
		if (systemStateLog[i].currNode == PAIRED && NetState.client[i].addr == currAddr) {
			i = 0;
			currAddr += 1;
		}
		if (currAddr >= MAX_ADDR) {
			break;
		}
	}
	
	if (currAddr >= MAX_ADDR) {
		printk("FATAL: No free network addresses to allocate!\n");
		mercury_led_fatal(12);
	}
	NetState.hm.active++; //it will be decreased in case node becomes lost
	printk("New node joined! Current number of nodes: %d\n",NetState.hm.active);
	NetState.client[nCli].addr = currAddr;
	printk("Assigned currAddr %d\n",NetState.client[nCli].addr);
	NetState.client[nCli].diag.last.joinCmbTs = k_uptime_get_32(); //joining timestamp of a node
	NetState.client[nCli].diag.isActive =1;
	return &NetState.client[nCli];
}

void printMeshHealth(){
    if(NetState.hm.currSys== STARTUP_NODATA){
        printk("Id,Node;Status\n");
	for(int i=0;i<NODES_NUM;i++){
	    printk("%d;%d;%s\n",i,NetState.client[i].addr,NODE_STATUS_STRING[systemStateLog[i].currNode]);
	}
	printk("-------------------------\n");
    }else{
        printk("Id, Node, PEC; Mismatch; Timeout, Dropped\n");
	for(int i=0;i<NODES_NUM;i++){
	    t_diag * diag= &NetState.client[i].diag;
	    printk("%d, %d, %d, %d, %d, %d\n",i,NetState.client[i].addr,diag->log.pecCnt,diag->log.tokenMisCnt,diag->log.timeoutCnt, diag->log.droppedCnt);
	}
	printk("-------------------------\n");
    }
    
}

void printConfigSt(){
      t_sysConf * config = &NetState.info.config;
      printk("Comm. settings\n");
      printk("Payload size: %d\n",config->pktSize);
      printk("Number of slaves: %d\n",config->nodesNum);
      printk("Number of meas. cells: %d\n",config->cellsNum);
      printk("Data Timeout: %d\n",config->cvTo);
      printk("OK Timeout: %d\n",config->okTo);
      printk("Data log: %d\n",LOG_LIMIT);
}

void sessionInfo(){
      t_misc * session = &NetState.info.session;
      session->startupTs = k_uptime_get_32();
}

void initConfigSt(){
      t_sysConf *config = &NetState.info.config;
      config->nodesNum = NODES_NUM;
      config->cellsNum = LTC_NCELLS;
      config->pktSize = PAYLOAD_SIZE;
      config->cvTo = DATA_TIMER_VALUE;
      config->okTo = OK_TIMER_VALUE;
      config->cellsNum = LTC_NCELLS;
}

#elif NODETYPE == NODETYPE_CMB

void mercury_netMgr_sendOK() {	
	mercury_mesh_sendOK();
}
#endif
//==============MSGQ================

#if NODETYPE == NODETYPE_BMS

struct t_lat data_pkt[NODES_NUM];

void mercury_netMgr_logWriteTime(uint8_t nodeAddr,t_pktS stage)
{
      uint8_t idx = nodeAddr-BASE_ADDR;
      uint32_t timestamp = k_uptime_get_32();
      
      switch(stage){
	  case RX_OK:
	      data_pkt[idx].timings.intervals.ok = timestamp;
	      data_pkt[idx].timings.cycleStart = timestamp;
	      break;
	  case TX_TOKEN:
	      data_pkt[idx].timings.intervals.tok=  timestamp;
	      break;
	  case RX_DATA:
	      data_pkt[idx].timings.intervals.cv = timestamp;
	      break;
	  case TX_ACK:
	      data_pkt[idx].timings.intervals.ack = timestamp;
	      data_pkt[idx].timings.cycleTime = timestamp - data_pkt[idx].timings.cycleStart;
	      data_pkt[idx].nid = nodeAddr;
	      size_t size = sizeof(struct t_lat);
	      timings_pktPtr = k_malloc(size); //new packet
	       if (timings_pktPtr != NULL) {
		    memcpy(timings_pktPtr, &data_pkt[idx], size);
		} else {
		    printf("ERR: Memory not allocated");
		}

		//printk("Radon PUT: %d, %d, %d, %d\n",  data_pkt[idx].nid, data_pkt[idx].timings.cycleTime,data_pkt[idx].timings.intervals.tok, data_pkt[idx].timings.intervals.cv);

		k_fifo_put(&hil_txqueue, timings_pktPtr);
		break;    
	  default:
		break;
      }
}


int message_dev_searchTX(t_rxCvPkt * rxmsg, t_txTokPkt *matched){
      t_txTokPkt txmsg;
      int curr_size = k_msgq_num_used_get(&tx_msgq);
      if(curr_size==0) return 0; //empty queue
      //ble_txqueue_info(); //show current size
      for(int i=0;i<curr_size;i++)
      {
	  int ret = k_msgq_get(&tx_msgq,&txmsg,K_NO_WAIT);
	  //printk("Comp: %d, %lu <--> %d, %lu\n",rxmsg->addr, rxmsg->token,txmsg.addr, txmsg.token);
	  if (ret == 0) {
	      if(txmsg.token == rxmsg->token && txmsg.addr == rxmsg->addr)
	      {
		    //printk("Found matching tx msg: %lu\n",txmsg.addr);
		    matched = &txmsg;
		    return 1;
	      }
	  }
	  else{
	      //printk("ERR: Empty tx msgq. Couldn't search\n");
	  }
      }
      //printk("WRN: No match found!\n");
      return 0;
}

int message_dev_fetchTX(t_txTokPkt *m)
{
	int ret = k_msgq_get(&tx_msgq, m, K_NO_WAIT); //waits forever 
	if (ret != 0) {
	//	printk("WRN: [TX] No MSG in the msg queue\n");
		return 0;
	}
	return 1;
}

void message_dev_pushQTX(t_txTokPkt *m)
{	
	int curr_size = k_msgq_num_used_get(&tx_msgq);
	if(curr_size>= SIZE_OF_TXQUEUE){
		printk("WRN: [TX] Queue full, Removing oldest\n");
		k_msgq_get(&tx_msgq, m, K_NO_WAIT); //remove oldest element
	} 
	int ret = k_msgq_put(&tx_msgq, m, K_NO_WAIT);
	if (ret != 0) {
		printk("ERR: [TX] Queue full, packet dropped!\n");
	}
}

int message_dev_fetchRX(t_rxCvPkt *m)
{
	int ret = k_msgq_get(&rx_msgq, m, K_NO_WAIT); //waits forever 
	if (ret != 0) {
		//printk("WRN: [RX] No MSG in the msg queue!\n");
		return 0;
	}
	return 1;
}

void message_dev_pushQRX(t_rxCvPkt *m)
{	
	int curr_size = k_msgq_num_used_get(&rx_msgq);
	if(curr_size>= SIZE_OF_RXQUEUE){
		printk("WRN: [RX] Queue full, Removing oldest\n");
		k_msgq_get(&rx_msgq, m, K_NO_WAIT); //remove oldest element
	} 
	int ret = k_msgq_put(&rx_msgq, m, K_NO_WAIT);
	if (ret != 0) {
		printk("ERR: [RX] Queue full, packet dropped!\n");
	}
}

void ble_rxqueue_info(void)
{
    struct k_msgq_attrs ble_attrs;
    k_msgq_get_attrs(&rx_msgq,&ble_attrs);
    int curr_size = ble_attrs.used_msgs;
    printk("[TX] %d msgs (of size %d [B]) out of %d used.\n",curr_size,ble_attrs.msg_size,ble_attrs.max_msgs);
}

void ble_txqueue_info(void)
{
    struct k_msgq_attrs ble_attrs;
    k_msgq_get_attrs(&tx_msgq,&ble_attrs);
    int curr_size = ble_attrs.used_msgs;
    printk("[RX] %d msgs (of size %d [B]) out of %d used.\n",curr_size,ble_attrs.msg_size,ble_attrs.max_msgs);
}

#elif NODETYPE == NODETYPE_CMB

int message_dev_fetchTX(t_tx *m)
{
	int ret = k_msgq_get(&tx_msgq, m, K_FOREVER); //waits forever 

	//__ASSERT_NO_MSG(ret == 0);
	if (ret != 0) {
		printk("ERROR: No MSG in the msg queue!\n");
	}

	return ret;
}

void message_dev_pushQTX(t_tx *m)
{	
	int curr_size = k_msgq_num_used_get(&tx_msgq);
	if(curr_size>= SIZE_OF_TXQUEUE){
		printk("Queue full, Removing oldest\n");
		k_msgq_get(&tx_msgq, m, K_FOREVER); //remove oldest element
	} 
	int ret = k_msgq_put(&tx_msgq, m, K_NO_WAIT);
	if (ret != 0) {
		printk("ERROR: Queue full, packet dropped!\n");
	}
}

int message_dev_fetchRX(t_rx *m)
{
	int ret = k_msgq_get(&rx_msgq, m, K_FOREVER); //waits forever 
	if (ret != 0) {
		printk("ERROR: No MSG in the msg queue!\n");
	}

	return ret;
}

void message_dev_pushQRX(t_rx *m)
{	
	int curr_size = k_msgq_num_used_get(&rx_msgq);
	if(curr_size>= SIZE_OF_RXQUEUE){
		printk("Queue full, Removing oldest\n");
		k_msgq_get(&rx_msgq, m, K_FOREVER); //remove oldest element
	} 
	int ret = k_msgq_put(&rx_msgq, m, K_NO_WAIT);
	if (ret != 0) {
		printk("ERROR: Queue full, packet dropped!\n");
	}
}

#endif

