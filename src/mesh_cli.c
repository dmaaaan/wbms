#include "mesh_cli.h"
#include "hg_netMgr.h"
#include "hg_mesh.h" //for the arrays. risk of multiple defs
#include "main.h"
#include "hg_ltc.h" //for getting fake cv
#include "fnv.h"

#if NODETYPE == NODETYPE_BMS
static void mercury_mesh_OKHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
static void mercury_mesh_DataHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
static void mercury_mesh_RXAddrRQHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
void bt_mesh_cli_sendACK(struct bt_mesh_model *model,uint16_t  destAddr);
void bt_mesh_cli_sendRecover(struct bt_mesh_model *model,uint16_t  destAddr);

#elif NODETYPE == NODETYPE_CMB
static void mercury_mesh_StartHandler(struct bt_mesh_model *model,struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf);
static void mercury_mesh_ACKHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf);
static void mercury_mesh_RecoveryHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf);
static void mercury_mesh_TokenHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
static void mercury_mesh_RXAddrRPHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
//OP initiating functions- called from hg_mesh
void bt_mesh_cli_request_address(struct bt_mesh_model *model);
void bt_mesh_cli_sendOK(struct bt_mesh_model *model);
void bt_mesh_cli_sendData(struct bt_mesh_model *model, t_tx *pkt);

//Net buffer defines
static struct net_buf_simple pkt_buf;
NET_BUF_SIMPLE_DEFINE_STATIC(pkt_buf,SDU_SIZE); //16+4+2=22B payload +3 +4
#endif


#if NODETYPE == NODETYPE_BMS
extern hgNetState NetState; //the same as in mesh.c

struct fifo_data_t {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	uint8_t addr;
	uint8_t cnt;
};
K_FIFO_DEFINE(addrFifo); //used to pass addresses in: OkHandler --> sendToken
K_FIFO_DEFINE(dataAddrFifo);

uint8_t addrCnt;
char *mem_ptr_ok;
char *mem_ptr_data;
#elif NODETYPE == NODETYPE_CMB
extern struct k_sem ack;
extern struct k_sem startSem;
extern struct k_sem pollSem;
extern hgLTC_State cmbData;
extern hgNetClient NetCli;
extern uint8_t ackSgn; //ok signal variable- see slave_app
extern uint8_t rcvSgn; //recovery signal variable
extern uint8_t startSgn;
extern uint16_t currCrc;
#endif

uint8_t endAddr = 255;

/*=======================OPDEFS==========================*/

 const struct bt_mesh_model_op vnd_ops[] = {
#if NODETYPE == NODETYPE_BMS
	{ OP_OK, 0, mercury_mesh_OKHandler},
	{ OP_DATA,  0,  mercury_mesh_DataHandler},
	{ OP_ADDR_RQ, 0, mercury_mesh_RXAddrRQHandler},
#elif NODETYPE == NODETYPE_CMB
	{ OP_ACK, 0, mercury_mesh_ACKHandler},
	{ OP_RCV, 0, mercury_mesh_RecoveryHandler},
	{ OP_START, 0, mercury_mesh_StartHandler},
	{ OP_TOKEN, 0, mercury_mesh_TokenHandler},
	{ OP_ADDR_RP, 0, mercury_mesh_RXAddrRPHandler},
#endif
	BT_MESH_MODEL_OP_END,
};


/*-----------------FOR USE ONLY locally------------------------------------------ */
#if NODETYPE == NODETYPE_BMS
static void mercury_mesh_sendAddress(struct bt_mesh_model *model, uint16_t newAddr, uint32_t tCode)
{
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = NODE_CMB_DEFADDR,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};

	BT_MESH_MODEL_BUF_DEFINE(msg, OP_ADDR_RP, 6);
	bt_mesh_model_msg_init(&msg, OP_ADDR_RP);

	net_buf_simple_add_le32(&msg, tCode);	
	net_buf_simple_add_le16(&msg, newAddr);

	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED while sending address to CMB\n");
		mercury_led_fatal(3);
	}

}
void bt_mesh_cli_sendToken(struct bt_mesh_model *model, uint8_t addr){
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	/*generate token and send it*/
	uint32_t hval, timestamp;
	timestamp= k_uptime_get_32();
	hval= token_gen(timestamp);
	
	/*send token part*/
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_TOKEN, TOKEN_SIZE);
	bt_mesh_model_msg_init(&msg, OP_TOKEN);
	net_buf_simple_add_le32(&msg, hval);
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk("ERR: FAILED while sending token\n");
		mercury_led_fatal(3);
	}
	else{ /*if success*/
	    printk(">>>SEND TOKEN to %d\n",addr);
	    updateTokenInfo(hval,addr);
	    mercury_netMgr_startDataTimer(addr-BASE_ADDR);
	}

}


/*we enter here from OKHandler or DataHandler. 
Token is sent after the last Ok was received or when Data from a node came. 
see bms transaction state machine*/
static void mercury_mesh_sendToken(struct bt_mesh_model *model) 
{
    struct fifo_data_t *sAddr;
	struct fifo_data_t *destAddr;
	/*LOGIC: addrFifo is filled up with CMB addresses
	 that just sent Ok messages. If we received all the OKs
	the same number of elements should be in FIFO
	 and we enter here to send a token.*/
	if(!k_fifo_is_empty(&addrFifo)) 
	{
	      sAddr = k_fifo_get(&addrFifo,K_NO_WAIT);
	      bt_mesh_cli_sendToken(model,sAddr->addr);
	      //flush fifo (k_malloc)
	      k_free(sAddr); 
	}
	else{	
	/*given the previous FIFO is empty (all the tokens were sent) 
	and we received data messages from all the nodes 
	(FIFO has elements), we will send all ACKs at once*/
	    while(!k_fifo_is_empty(&dataAddrFifo)){ 
			destAddr = k_fifo_get(&dataAddrFifo,K_NO_WAIT);
			bt_mesh_cli_sendACK(model, destAddr->addr);
			k_free(destAddr);
	    }
	}
}
#endif
/*---------------------------HANDLES------------------------------------------ */

#if NODETYPE == NODETYPE_CMB

static void mercury_mesh_StartHandler(struct bt_mesh_model *model,struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf)
{
      k_sem_give(&startSem);
      startSgn =1;
}

static void mercury_mesh_ACKHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf)
{
      uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
      if (ctx->addr == currmeshAddr) {
	      return;
      }
      ackSgn= 1;

}

static void mercury_mesh_RecoveryHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,struct net_buf_simple *buf)
{
      uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
      if (ctx->addr == currmeshAddr) {
	      return;
      }
      rcvSgn= 1;
}

static void mercury_mesh_TokenHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf)
{
	t_tx newPkt;
	uint32_t token;

	uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
	if (ctx->addr == currmeshAddr) { //check if token came from valid master node
		return; //drop handling of the message
	}

	token = net_buf_simple_pull_le32(buf); //acwuire incoming token
	newPkt.token= token;
	newPkt.ts = k_uptime_get_32(); //timestamp used for internal logging

	net_buf_simple_reset(buf); //clear NET buffer to avoid future jams 

	k_sem_take(&pollSem,K_FOREVER); //claim access to memory address
	newPkt.cv = &cmbData.cellVolts; //fetch newest battery data
	newPkt.crc = currCrc; //fetch corresponding crc16 value
	k_sem_give(&pollSem);//release semaphore

	bt_mesh_cli_sendData(model,&newPkt);//send data to master node
	updateTokenInfo();    //save cmb info for internal diagnostics
}

//Called upon receiving an addressRQ reply packet
static void mercury_mesh_RXAddrRPHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf)
{
	uint16_t currhwAddr= mercury_mesh_getHwAddr();
	uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
	
	if (ctx->addr == currmeshAddr) {
		return;
	}

	union {
		uint8_t b[2];
		uint16_t u16;
	} u2;
	
	union {
		uint8_t b[4];
		uint32_t u32;
	} u4;
	
	for (uint8_t i = 0; i < 4; i++) {
		u4.b[i] =  buf->data[i];
	}
	
	for (uint8_t i = 0; i < 2; i++) {
		u2.b[i] =  buf->data[i+4];
	}

	//Ensure this packet is for this node
	if (u4.u32 != NetCli.addrToken) {
		return;
	}
	
	bt_mesh_reset();

	//printk("hwAddr: %d\n",currhwAddr); 

	//unbind from existing model to rebind with new params
	bt_mesh_cfg_app_key_del(net_idx, currhwAddr, net_idx, app_idx, NULL);
	bt_mesh_cfg_mod_app_unbind_vnd(net_idx, currhwAddr, currhwAddr, app_idx, MOD_LF, BT_COMP_ID_LF, NULL);
	bt_mesh_cfg_mod_app_unbind(net_idx, currhwAddr, currhwAddr, app_idx, BT_MESH_MODEL_ID_HEALTH_SRV, NULL);
	bt_mesh_cfg_mod_sub_del_vnd(net_idx, currhwAddr, currhwAddr, GROUP_ADDR, MOD_LF, BT_COMP_ID_LF, NULL);
	
	mercury_mesh_updateAddr(u2.u16,u2.u16);
	//updated values imply that we updated these variables too
	currmeshAddr= mercury_mesh_getMeshAddr();
	currhwAddr= mercury_mesh_getHwAddr();
	
	if (bt_mesh_provision(net_key, net_idx, flags, iv_index, currhwAddr, dev_key) != 0) {
		//printk("Error during reprovisioning\n");
	}
	
	bt_mesh_cfg_app_key_add(net_idx, currhwAddr, net_idx, app_idx, app_key, NULL);

	/* Bind to vendor model */
	bt_mesh_cfg_mod_app_bind_vnd(net_idx, currhwAddr, currhwAddr, app_idx, MOD_LF, BT_COMP_ID_LF, NULL);

	/* Add model subscription */
	bt_mesh_cfg_mod_sub_add_vnd(net_idx, currhwAddr, currhwAddr, GROUP_ADDR, MOD_LF, BT_COMP_ID_LF, NULL);

}

#elif NODETYPE == NODETYPE_BMS

static void mercury_mesh_OKHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf)
{
	//get frame counter value. 
	int pkt_cnt = net_buf_simple_pull_u8(buf);
	//get senders address
	uint8_t okAddr= ctx->addr;
	//check if state machine is correupted
	uint8_t statesErr = updateOKInfo(buf, okAddr);
	size_t size = sizeof(struct fifo_data_t);
    struct fifo_data_t addrData = { 
	.addr = okAddr, 
	.cnt = addrCnt };		

	uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
    if (okAddr == currmeshAddr) {
	    return;
    }  

	if(!statesErr)
	{
		//timer is stopped when OK msg arrives
	    mercury_netMgr_stopOkTimer(okAddr);
	    mem_ptr_ok = k_malloc(size);

	    if (mem_ptr_ok != NULL) {
		memcpy(mem_ptr_ok, &addrData, size);
	    } else {
		printf("ERR: Memory not allocated");
	    }
	    k_fifo_put(&addrFifo, mem_ptr_ok);//put the addresses in order in fifo to send the tokens to them 
	    //printk("Check\n");
	    
	    addrCnt++;

	    if(addrCnt==NetState.hm.active){ //active can be different than the predefined size
		  mercury_netMgr_updateSystemState(NO_ISSUES); //ok received from all the nodes->NO_ISSUES
		  mercury_mesh_sendToken(model);
		  addrCnt=0;
	    }

      }
      else {
	  updateRejectedMsgInfo(okAddr);
      }
}

void putDataFifo(struct fifo_data_t data){
	size_t size = sizeof(struct fifo_data_t);
	mem_ptr_data = k_malloc(size);

	if (mem_ptr_data != NULL) {
	  memcpy(mem_ptr_data, &data, size);
	} else {
	  printf("Memory not allocated");
	}
	k_fifo_put(&dataAddrFifo, mem_ptr_data);//used by sendToken() next

}

static void mercury_mesh_DataHandler(
 struct bt_mesh_model *model,
 struct bt_mesh_msg_ctx *ctx, 
 struct net_buf_simple *buf)
{
	uint8_t dataAddr = ctx->addr;
	static uint8_t datmsgCnt;
	struct fifo_data_t msgData={ 
  	.addr = dataAddr, 
  	.cnt = datmsgCnt}; 
	/*if system ia in BATT_INFO_LOST state
	 then dont accept Data msgs because
	  we have to wait for Ok message*/
	if(NetState.hm.currSys != BATT_INFO_LOST)
	{
		//check current state with respect to previous
	    uint8_t statesErr =updateDataInfo(buf, dataAddr);
	    //enter if the state machine is not corrupted
	    if(!statesErr)
	    {
	    //timer is stopped when Data msg arrives
		  mercury_netMgr_stopDataTimer(dataAddr);
		  //keep track how many msg are in fifo
		  datmsgCnt++;
		  //state machine uses fifo to update itself
		  putDataFifo(msgData);
		  //send token to another slave node
		  mercury_mesh_sendToken(model);
	    }
	    else { 
	/*otherwise if state machine does not
	 recognise this message in the sequence
	 then drop the message and report it*/
		updateRejectedMsgInfo(dataAddr);
	    }
	}
	else{
	    updateRejectedMsgInfo(dataAddr);
	}

}
//Called upon receiving an addressRQ request packet
static void mercury_mesh_RXAddrRQHandler(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf)
{
	uint16_t currmeshAddr= mercury_mesh_getMeshAddr();
	
	if (ctx->addr == currmeshAddr) {
		return;
	}

	//This function is only meaningful on the BMS
	
	union {
		uint8_t b[4];
		uint32_t u32;
	} u4;
	
	for (uint8_t i = 0; i < 4; i++) {
		u4.b[i] =  buf->data[i];
	}
	
	uint32_t tokenCode = u4.u32;
	
	hgNetClient *newCli = mercury_netMgr_NewClient(); //NetState is passed as extern global variable
	
	printk("ADDR: Got an address config request. New address: %d, token=%d\n", newCli->addr, tokenCode);
	
	mercury_mesh_sendAddress(model,newCli->addr, tokenCode);
}
#endif

/*=================ACCESSIBLES================================*/

/*Functions initiating OP Handling at the receiver*/

#if NODETYPE == NODETYPE_CMB


void bt_mesh_cli_sendData(struct bt_mesh_model *model, t_tx * pkt)
{
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = NODE_BMS_ADDR,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};

	////	for(int i=0;i<16;i++){
	////    printk("%d, ",cv[i]);
	////}
	////printk("\n");
	bt_mesh_model_msg_init(&pkt_buf, OP_DATA);
	net_buf_simple_add_mem(&pkt_buf,pkt->cv,DATA_SIZE); //faster than below. 16B for cv
	net_buf_simple_add_le32(&pkt_buf,pkt->token);//the rest was added earlier. it cant be moved anywhere else
	net_buf_simple_add_le16(&pkt_buf,pkt->crc);
	
	if (bt_mesh_model_send(model, &ctx, &pkt_buf, NULL, NULL)) {
		printk(">>>FAILED while sending direct message response\n");
		mercury_led_fatal(3);
	}
}

void bt_mesh_cli_sendOK(struct bt_mesh_model *model)
{
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = NODE_BMS_ADDR,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	static int packet_count;
	
	
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_OK,1);
	bt_mesh_model_msg_init(&msg, OP_OK);
	net_buf_simple_add_u8(&msg, packet_count);

	//printk(">>> SEND OK\n");
	
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED while sending OK\n");
		mercury_led_fatal(3);
	}
	packet_count++;
}

void bt_mesh_cli_request_address(struct bt_mesh_model *model)
{
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = NODE_BMS_ADDR,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	
	NetCli.addrToken = sys_rand32_get();

	if(DEBUG_INFO) printk("rqAddr got token %d\n", NetCli.addrToken);

	BT_MESH_MODEL_BUF_DEFINE(msg, OP_ADDR_RQ, 4);
	bt_mesh_model_msg_init(&msg, OP_ADDR_RQ);
	
	if(DEBUG_INFO){
	    printk("rqAddr blank: ");
	    dumpMsg(net_buf_data_msg, msg.size);
	}

	net_buf_simple_add_le32(&msg, NetCli.addrToken);

	if(DEBUG_INFO){
	      printk("rqAddr code : ");
	      dumpMsg(net_buf_data_msg, msg.size);
	      printk(" complete\n");
	      printk(">>> SEND\n");
	}
	
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED send of address request message\n");
		mercury_led_fatal(3);
	}
}


#elif NODETYPE == NODETYPE_BMS

void bt_mesh_cli_flushFifos(){
    struct fifo_data_t *sAddr;
    struct fifo_data_t *destAddr;
    while(!k_fifo_is_empty(&addrFifo)) 
    {
	  sAddr = k_fifo_get(&addrFifo,K_NO_WAIT);
	  k_free(sAddr); //k_malloc allocated for addrFifo. release
    }
    while(!k_fifo_is_empty(&dataAddrFifo)){ 
	destAddr = k_fifo_get(&dataAddrFifo,K_NO_WAIT);
	k_free(destAddr);
    }
    printk("WRN: Flushed fifos\n");
}


void bt_mesh_cli_allowStart(struct bt_mesh_model *model, uint16_t destAddr)
{
	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = destAddr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	
	
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_START, 0);
	bt_mesh_model_msg_init(&msg, OP_START);
	addrCnt=0; //used by OkHandler
	
	
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED while sending start signal\n");
		mercury_led_fatal(3);
	}
}

void bt_mesh_cli_sendACK(struct bt_mesh_model *model, uint16_t destAddr)
{

	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = destAddr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	
	
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_ACK,0);
	bt_mesh_model_msg_init(&msg, OP_ACK);

	
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED while sending ACK msg\n");
		mercury_led_fatal(3);
	}
	else{
	      updateACKInfo(destAddr);
	      uint8_t nodeIdx = destAddr-BASE_ADDR;
	      mercury_netMgr_startOkTimer(nodeIdx);
	}
}

void bt_mesh_cli_sendRecover(struct bt_mesh_model *model, uint16_t destAddr)
{

	struct bt_mesh_msg_ctx ctx = {
		.app_idx  = app_idx,
		.addr	  = destAddr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
	};
	
	
	BT_MESH_MODEL_BUF_DEFINE(msg, OP_RCV,0);
	bt_mesh_model_msg_init(&msg, OP_RCV);

	
	if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
		printk(">>>FAILED while sending ACK msg\n");
		mercury_led_fatal(3);
	}
	else{
	      updateRecoverInfo(destAddr);
	}
}

#endif

