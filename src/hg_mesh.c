#include <kernel.h>
#include "hg_netMgr.h"
#include "mesh_cli.h"
#include "hg_mesh.h"
#include "hg_ltc.h" //for poll_fake_ltc

#if NODETYPE == NODETYPE_BMS
extern hgNetState NetState;
extern t_bmsSt systemStateLog[NODES_NUM];

void mercury_mesh_sendStart();
void mercury_mesh_resendACK(uint8_t nodeIdx);
void mercury_mesh_init();

uint16_t msgCnt;
uint8_t rxCounter;
uint32_t ok,tok,cv,ack,cycle;
t_rxCvPkt m;
extern uint8_t timer_started_node;

#elif NODETYPE == NODETYPE_CMB
t_logCmb cmbInfo;
extern hgNetClient NetCli;


void mercury_mesh_updateAddr(uint16_t newMeshAddress, uint16_t newHwAddress);
void mercury_mesh_requestAddress();
void mercury_mesh_sendOK();
#endif

uint16_t mercury_mesh_getHwAddr();
uint16_t mercury_mesh_getMeshAddr();

///*=====================HANDLES==============================*/
#if NODETYPE == NODETYPE_BMS

//void mercury_pingRX_handler(uint32_t cmbAddr, int pingTime){

//	uint32_t timestamp = k_uptime_get_32();

//	//int ret = k_msgq_get(&nodes[id].msgq, &m, K_FOREVER);
//	uint32_t sender =cmbAddr-BASE_ADDR; //0-15
//	//m.seq = msg_seq++; //Used for breaking the recording process
//	uint32_t latency= timestamp - pingTime;

//	//mercury_netMgr_record_latencies(&m);

//	printk("Ping test: ping to addr %d took %d ms\n", sender, latency);
	
//}

#endif

///*=====================HANDLES END==============================*/

static struct bt_mesh_cfg_cli cfg_cli = {};

static struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_SRV,
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
};

BT_MESH_MODEL_PUB_DEFINE(vnd_pub, NULL, 3+1); //used by ping etc here that need just one byte

static struct bt_mesh_model vnd_models[] = {
	BT_MESH_MODEL_VND(BT_COMP_ID_LF, MOD_LF, vnd_ops, &vnd_pub, NULL),
};

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, root_models, vnd_models),
};

static const struct bt_mesh_comp comp = {
	.cid		= BT_COMP_ID_LF,
	.elem		= elements,
	.elem_count = ARRAY_SIZE(elements),
};


/*===================================================================*/

static void configure(void)
{

	printk("Configuring...\n");
	/* Add Application Key */
	bt_mesh_cfg_app_key_add(net_idx, hwAddr, net_idx, app_idx, app_key, NULL);

	/* Bind to vendor model */
	bt_mesh_cfg_mod_app_bind_vnd(net_idx, hwAddr, hwAddr, app_idx, MOD_LF, BT_COMP_ID_LF, NULL);

	/* Add model subscription */
	bt_mesh_cfg_mod_sub_add_vnd(net_idx, hwAddr, hwAddr, GROUP_ADDR, MOD_LF, BT_COMP_ID_LF, NULL);

	printk("Configuration complete\n");
}

/*========================FUNCTIONS REFERENCED EXTERNALLY==============================*/

static void mercury_mesh_BTReady(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_mesh_init(&prov, &comp);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	printk("Mesh initialized\n");

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading stored settings\n");
		settings_load();
	}

	err = bt_mesh_provision(net_key, net_idx, flags, iv_index, hwAddr, dev_key);
	if (err == -EALREADY) {
		printk("Using stored settings\n");
	} else if (err) {
		printk("Provisioning failed (err %d)\n", err);
		return;
	} else {
		printk("Provisioning completed\n");
		configure();
	}
}


void mercury_mesh_updateAddr(uint16_t newMeshAddress, uint16_t newHwAddress)
{	
	meshAddr = newMeshAddress;
	hwAddr = newHwAddress;
	UICR_Customer0 = meshAddr;
	if(DEBUG_INFO) printk("(!) Address updated to(mesh,hw): [%d,%d]\n UICR_Customer0: %d\n",meshAddr,hwAddr,UICR_Customer0);
}

uint16_t mercury_mesh_getHwAddr(){ 
      return hwAddr;
} 

#if NODETYPE == NODETYPE_CMB
uint16_t mercury_mesh_getBmsAddr(){ 
      return bmsAddr;
} 


void mercury_mesh_requestAddress(void){
      bt_mesh_cli_request_address(&vnd_models[0]);
}


void mercury_mesh_sendOK(){
      bt_mesh_cli_sendOK(&vnd_models[0]);
      updateOKInfo();	//update cmb info
};
#endif

uint16_t mercury_mesh_getMeshAddr()
{	
	return meshAddr;
}

#if NODETYPE == NODETYPE_BMS

void mercury_mesh_recoverNodes(int nodeIdx){
      uint16_t destAddr;
      destAddr =NetState.client[nodeIdx].addr;
      bt_mesh_cli_sendRecover(&vnd_models[0], destAddr);
}

void mercury_mesh_resendACK(uint8_t nodeIdx){
      uint16_t destAddr;
      destAddr =NetState.client[nodeIdx].addr;
      bt_mesh_cli_sendACK(&vnd_models[0], destAddr);
      printk("WRN: Resending to %d\n", destAddr);
}

void mercury_mesh_sendStart()
{
    for (uint8_t i = 0; i < NetState.hm.active; i++) 
    {
	  /*either a node has to be paired or the system has to be in dangerous state */
	  if (systemStateLog[i].currNode == PAIRED || NetState.hm.currSys ==BATT_INFO_LOST) 
	  {    
		  bt_mesh_cli_allowStart(&vnd_models[0], NetState.client[i].addr);
		  printk(">>> GO %d!\n",NetState.client[i].addr);
	  }
	  else{
	      printk("ERR: Node/System %d in WRONG state to start/restart\n");
	  }
    }  
}

void mercury_mesh_resendToken(uint8_t addr){
    bt_mesh_cli_sendToken(&vnd_models[0], addr);
}

void updateRejectedMsgInfo(uint16_t dst){
      uint8_t nodeIdx=dst-BASE_ADDR; //node indexing should be mapped to array indexing
      t_diag * diag= &NetState.client[nodeIdx].diag;
      mercury_netMgr_errorCountUpdate(dst,PKT_ST_ERR);
}

void updateRecoverInfo(uint16_t dst)
{
	mercury_netMgr_statesUpdate(dst, TX_RECOVER);
	mercury_netMgr_packetCountUpdate(dst, TX_RECOVER);
}

void updateACKInfo(uint16_t dst)
{
	mercury_netMgr_statesUpdate(dst, TX_ACK);
	mercury_netMgr_logWriteTime(dst,TX_ACK);
	mercury_netMgr_packetCountUpdate(dst, TX_ACK);
	mercury_netMgr_updateSystemState(NO_ISSUES); //first cycle completed- we can write NO_ISSUES
}

uint8_t updateDataInfo(struct net_buf_simple * buf, uint16_t senderAddr)
{
	uint8_t statesErr;
	statesErr=mercury_netMgr_statesUpdate(senderAddr, RX_DATA);

	if(!statesErr)
	{
	    char rxCv[100];
	    char *bufPtr = rxCv; 
	    mercury_netMgr_logWriteTime(senderAddr,RX_DATA);
	

	    m.addr = senderAddr;
	    m.ts = k_uptime_get_32();

	    for(int i=0;i<LTC_NCELLS;i++) 
	    {
		  m.data[i] = net_buf_simple_pull_u8(buf); //get cv
		  bufPtr+= sprintf(bufPtr,"%d,", m.data[i]); 
	    }
	    m.token = net_buf_simple_pull_le32(buf); //get token
	    m.pec =  net_buf_simple_pull_le16(buf); 
	
	    printk("%d: RX CV: %s from %d\n",msgCnt, rxCv,senderAddr);
	    printk("%d: RX TOKEN: %lu\n",msgCnt,m.token);
	    printk("%d: RX PEC: %d\n",msgCnt, m.pec);
	    printk("\n");
	    message_dev_pushQRX(&m); //this message goes to diag_fn
	    msgCnt++;
	    mercury_netMgr_packetCountUpdate(senderAddr, RX_DATA);
	}
	return statesErr;
}

uint8_t updateOKInfo(struct net_buf_simple * buf, uint16_t senderAddr)
{
	uint8_t statesErr;
	statesErr = mercury_netMgr_statesUpdate(senderAddr, RX_OK);
	NetState.client[senderAddr-BASE_ADDR].diag.lastReceivedOk = k_uptime_get_32(); // for error counter info 

	if(!statesErr){
	    mercury_netMgr_logWriteTime(senderAddr,RX_OK);
	    rxCounter = net_buf_simple_pull_u8(buf); //pull the counter from ok message into the struct. not used further as of now.
	    mercury_netMgr_transactionUpdate(senderAddr, STARTED);
	    mercury_netMgr_packetCountUpdate(senderAddr, RX_OK);
	}
	return statesErr;
}

void updateTokenInfo(uint32_t token, uint16_t dst){
	t_txTokPkt msg;

	msg.token = token; //update tx msg info log
	msg.ts = k_uptime_get_32();
	msg.rxCnt = rxCounter; //global variable
	msg.addr = dst;


	mercury_netMgr_logWriteTime(dst,TX_TOKEN);



	mercury_netMgr_statesUpdate(dst, TX_TOKEN);

	message_dev_pushQTX(&msg); //this message goes to listen_fn
	mercury_netMgr_packetCountUpdate(dst, TX_TOKEN);
}
#elif NODETYPE == NODETYPE_CMB

void updateOKInfo(){
    cmbInfo.last.lastTxOk = k_uptime_get_32();

}

void updateTokenInfo(){
    cmbInfo.last.lastRxTok = k_uptime_get_32();
}

#endif

static inline void board_init()
{

	if (!UICR_Customer0 || UICR_Customer0 == 0xffff) {
	
	#if NODETYPE == NODETYPE_CMB
		UICR_Customer0 = NODE_CMB_DEFADDR;
		meshAddr = NODE_CMB_DEFADDR;
		hwAddr = NODE_CMB_DEFADDR;
	#endif
	
	#if NODETYPE == NODETYPE_BMS
		UICR_Customer0 = NODE_BMS_ADDR;
		meshAddr = NODE_BMS_ADDR;
		hwAddr = NODE_BMS_ADDR;
	#endif

	}

	printk("mesh address: %d/%d\n", meshAddr, hwAddr);
}



void mercury_mesh_init()
{
	printk("mercury_mesh_init\n");
	int err;

	board_init();


	err = bt_enable(mercury_mesh_BTReady);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
}