#include "hg_test.h"
#include "hg_netMgr.h"
#include "led.h"
#include "hg_mesh.h"


#if NODETYPE == NODETYPE_BMS

void print_health_report();
void print_data_bucket();
void print_summary();

struct {
    unsigned int nid  : 4; //node address
    unsigned int startOk: 32; //cycle start timestamp
    unsigned int tok : 32; //diff in ms from startOk
    unsigned int cv : 32;//diff
    unsigned int ack : 32; //diff
    unsigned int padding: 4; //84+4
}timingPkt;

struct {
    unsigned int nid : 4;
    unsigned int timestamp : 32;
    unsigned int started : 16; // diff since last one
    unsigned int closed : 16;
    unsigned int rejected: 16; //combined of all the below
    unsigned int pec : 6;
    unsigned int timeout : 6;
    unsigned int mismatch : 6;
    unsigned int lost : 4; //how many times node was lost within this time
    unsigned int padding : 6; //74+6
}cntPkt;

struct{
    unsigned int pkt: 4; //pkt size
    unsigned int nodes: 3; 
    unsigned int cells: 4;
    unsigned int timeout: 16; //in ms
    unsigned int startup: 32;
    unsigned int padding: 3;
}configPkt;

struct{
    unsigned int nid: 4;
    unsigned int joined: 24; //diff with startup in ms
    unsigned int padding: 2;
}nodeInfo;

struct{
    unsigned int nid : 4;
    unsigned int time : 24; //diff with startup in ms
    unsigned int code : 4; //encoded enum
}bmsMsg;

extern hgNetState NetState; //all the info goes in here
extern t_transCnt transLog[NODES_NUM];
extern t_pktCnt  pktCntLog[NODES_NUM];
extern t_bmsSt systemStateLog[NODES_NUM];

uint8_t zeros_cnt;

 t_pktCnt lasttx_PktCnt;
t_errCnt lasttx_ErrCnt;
t_transCnt lasttx_transCnt;


void radon_latenciesInfoTX(){

    struct t_lat * fifoPtr;

    if(!k_fifo_is_empty(&hil_txqueue)){ 
        fifoPtr = k_fifo_get(&hil_txqueue, K_NO_WAIT);
	//printk("Radon get: %d,%d, %d, %d\n",fifoPtr->nid, fifoPtr->timings.cycleTime, fifoPtr->timings.intervals.tok,fifoPtr->timings.intervals.cv);
	timingPkt.ack = fifoPtr->timings.intervals.ack;
	timingPkt.cv = fifoPtr->timings.intervals.cv;
	timingPkt.tok = fifoPtr->timings.intervals.tok;	
	timingPkt.startOk = fifoPtr->timings.intervals.ok;
	timingPkt.nid = fifoPtr->nid;
	//printk("Radon get: %d, %d, %d\n",timingPkt.nid, timingPkt.startOk, timingPkt.ack);
	//once data arrived pass it quickly to Radon
	k_free(fifoPtr);
    }
    
}

void radon_configInfoTX(){
	configPkt.cells = NetState.info.config.cellsNum;
	configPkt.nodes = NetState.info.config.nodesNum;
	configPkt.startup = NetState.info.session.startupTs;
	configPkt.pkt = NetState.info.config.pktSize;
	//send
}

void radon_countersInfoTX(){
      for(int i=0;i<NODES_NUM;i++)
      {
	  cntPkt.nid = i+BASE_ADDR;
	  cntPkt.closed = transLog[i].tClosed;
	  cntPkt.rejected = transLog[i].tRejected;
	  cntPkt.started = transLog[i].tStarted;

	  //cntPkt.lost = 
	  cntPkt.mismatch = NetState.client[i].diag.log.tokenMisCnt;
	  cntPkt.pec = NetState.client[i].diag.log.pecCnt;
	  //cntPkt.timeout

	  cntPkt.timestamp = NetState.client[i].diag.lastUpdated;
		
	  //send
      }
}

void radon_inits(){
}


void print_summary()
{
    printk("Transaction summary\n");
    printk("Node, started, closed, rejected\n");
    for(int i=0;i<NODES_NUM;i++)
	printk("%d|%d|%d|%d\n",i,transLog[i].tStarted,transLog[i].tClosed,transLog[i].tRejected);
    printk("----------------------\n");

    printk("Packet summary\n");
    printk("Node, Ok, Token, Data, Ack\n");
    for(int i=0;i<NODES_NUM;i++)
	printk("%d|%d|%d|%d|%d\n",i,pktCntLog[i].OkCnt,pktCntLog[i].TokCnt,pktCntLog[i].CvCnt, pktCntLog[i].AckCnt);
    printk("----------------------\n");
}
#endif