#ifndef HG_NETMGR_H
#define HG_NETMGR_H

#include <stdint.h>
#include <zephyr.h>
#include <random/rand32.h>

#include "buildcfg.h"
#include "hg_ltc.h"
#include "hg_test.h"


#define HG_NET_MAXCLIENTS	16
#define HG_NET_FLAG_ACTIVE	1 << 0

#define MAX_EVENTS 5
#define LOG_LIMIT 4 //size of data buffer for storing logs.
#define CALC_STATS_SAMPLES    LOG_LIMIT //from how many cycles one statistics entry is calculated
#define LOG_SBUF 2 //2 additional slots just in case we dont make flushing in 4
#define	LOG_ARR (LOG_LIMIT+LOG_SBUF)


#define BASE_ADDR 16
#define MAX_ADDR  221

#define ALL_NODES 255

//====MSGQ====
#define NODES_NUM 1 //<HG_NET_MAXCLIENTS
#define STAGE_CNT 4 //PrevAckOk,OkTok,TokCv,CvAck

#define DATA_RX_PERIOD 1000 //every 1000 ms get all the data
#define OK_STATUS_BYTES	1//1 Byte just for status counter
#define OK_STATUS_INTERVAL_MS 20
#define OK_INTSTEPS   (OK_STATUS_INTERVAL_MS/100)//divided by 100ms

#define DATA_SIZE (LTC_NCELLS*CV_SIZE) //packet size should be automatically fitted to the number of cells
#define PEC_SIZE 2// crc16 = 2B
#define PAYLOAD_SIZE (DATA_SIZE+TOKEN_SIZE+PEC_SIZE)
#define NODE_INTERVAL (DATA_RX_PERIOD/NODES_NUM) // interval is a function of number of nodes. otherwise packets will be lost
#define MIC_SIZE 4 //can be either 4 or 8. only using 4B recommended
#define SEQ_SIZE 3 //unique fast-changing part of the message. fixed size
#define SDU_SIZE (SEQ_SIZE+PAYLOAD_SIZE+MIC_SIZE)

#define OK_TIMER_VALUE 1000
#define DATA_TIMER_VALUE 1000

/* Amount of packet headers in a queue */
#define SIZE_OF_RXQUEUE 6
#define SIZE_OF_TXQUEUE 40
#define TOKEN_SIZE 4

/*all the chunked stuff*/
//#define MSG_CHUNK 8
//#define TOKCHUNKS_N (TOKEN_SIZE/CHUNK_SIZE)+((TOKEN_SIZE%CHUNK_SIZE)==0? 0:1)
//#define CHUNKS_N (DATA_SIZE/CHUNK_SIZE)+((DATA_SIZE%CHUNK_SIZE)==0? 0:1) //has to be separate from token_size because they are fragmented at differnet stage
//#define CHUNK_SIZE 4 //instead of 16 due to fragmentation. resolve in thread with ble_tx queue
//#define CHUNKED 0 //switch on fragmentation. Deprecated

#define CMB_MSG_TIMEOUT 5000

#if NODETYPE == NODETYPE_BMS
//below defines are used to turn enum into strings and print them
/*status as per the latest transaction event seen by BMS- pattern repeated every full transaction:
1. no info at startup, 2.rx ok, 3. tx token 4. rx data, 5. go to 2. etc. NO_INFO is given again after exceeding timeout */
#define FOREACH_PKT_STATUS(PKT_STATUS) \
	PKT_STATUS(NO_INFO)	\
	PKT_STATUS(RX_OK)  \
	PKT_STATUS(TX_TOKEN)  \
	PKT_STATUS(TX_ACK)    \
	PKT_STATUS(RX_DATA)  \
	PKT_STATUS(TX_RECOVER)  \
//comments
///*from above statuses + timestamps we derive more general diagnostic information about each node*/
//INACTIVE not paired yet- warning
//PAIRED at startup
//ACTIVE all good
//DATA_LOSS if a node doesnt send CV i.e RX_DATA is not entered for more than DATA_TIMEOUT- warning
//LOST if a node has NO_INFO status i.e. doesnt send OK for more than OK_TIMEOUT- warning
//!!!!!!!comments not allowed next to below defines cuz it becomes \//
#define FOREACH_NODE_STATUS(NODE_STATUS)  \
        NODE_STATUS(INACTIVE)  \
        NODE_STATUS(PAIRED)  \
        NODE_STATUS(ACTIVE)  \
	NODE_STATUS(TOKEN_MISMATCH)  \
	NODE_STATUS(PEC_FAILED)	\
	NODE_STATUS(TIMEOUT_ERR) \
	NODE_STATUS(TRUST_ERR)	\
	NODE_STATUS(PKT_ST_ERR)	\
        NODE_STATUS(LOST)  \
/*
BATT_INFO_LOST if no info is av atm- error
BATT_INFO_RECOVERED if info was lost and came back without violating deadlines
NO_ISSUES no losses occured since startup
CNTR_OPENED if system was in cntr open state at any time- error
STARTUP_NODATA
*/
#define FOREACH_SAFETY_STATUS(SAFETY_STATUS) \
	SAFETY_STATUS(BATT_INFO_LOST) \
	SAFETY_STATUS(NO_ISSUES) \
	SAFETY_STATUS(STARTUP_NODATA) \


#define FOREACH_TRANS_STATUS(TRANS_STATUS) \
	TRANS_STATUS(STARTED) \
	TRANS_STATUS(REJECTED) \
	TRANS_STATUS(ENDED) \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

/*Packet Status enum*/
typedef enum{
	FOREACH_PKT_STATUS(GENERATE_ENUM)
}t_pktS;

/*Node Status enum*/
typedef enum{
      FOREACH_NODE_STATUS(GENERATE_ENUM)
}t_nodeS;

/*System Safety Status enum*/
typedef enum{
      FOREACH_SAFETY_STATUS(GENERATE_ENUM)
}t_sysSS;

typedef enum{
      FOREACH_TRANS_STATUS(GENERATE_ENUM)
}t_transS;

static const char *SAFETY_STATUS_STRING[] = {
    FOREACH_SAFETY_STATUS(GENERATE_STRING)
};

static const char *PKT_STATUS_STRING[] = {
    FOREACH_PKT_STATUS(GENERATE_STRING)
};

static const char *NODE_STATUS_STRING[] = {
    FOREACH_NODE_STATUS(GENERATE_STRING)
};

//--------------------------------------------------------------



/*diagnostic summary for counting number of occurances of several safety violations*/
struct diagSum{
      uint32_t contactorOpenCntr; //counter of contactor opened states
      uint32_t nodeLostCntr; //how many times node connection was lost (ok not rx)
      uint8_t nodesLost; //how many nodes where lost 0- NODES_NUM
      uint32_t maxLostT; //time without ok message by a node
      uint32_t avgLostT;
      uint32_t runtime;
      uint32_t errMsgCntr; //count number of error msgs printed
      uint32_t wrnMsgCntr;
};

typedef struct{
    t_pktS prevPkt;
    t_pktS currPkt;
    t_nodeS prevNode;
    t_nodeS currNode;
}t_bmsSt;

typedef  struct{
    uint32_t tStarted;
    uint32_t tClosed;
    uint32_t tRejected;
}t_transCnt;

typedef struct{
    uint32_t TokCnt;
    uint32_t CvCnt;
    uint32_t OkCnt;
    uint32_t AckCnt;
    uint32_t RcvCnt; //recoveries
    uint32_t RstCnt; //restarts, starts
}t_pktCnt;

typedef struct{
      uint32_t pecCnt;
      uint32_t timeoutCnt;
      uint32_t tokenMisCnt;
      uint32_t droppedCnt; //messages that were received but were dropped due to uncorrect moment in state machine
}t_errCnt;

/*system configuration parameters exported to Radon*/
typedef struct{
	uint8_t pktSize;      //bytes of payload
	uint32_t okTo;	     //timeout settings
	uint32_t cvTo;
	uint8_t nodesNum;
	uint8_t cellsNum;
}t_sysConf;

/*all other info gathered by BMS*/
typedef struct{
	uint32_t startupTs; //begining of main() timestamp for BMS
	uint32_t sessionTs; //common timebase for all nodes- if time synchronisation is used
	/*add more*/
}t_misc;

typedef struct {
	t_misc session;
	t_sysConf config;
}t_set;

typedef struct { //this pkt is sent to a node that sent OK msg in the first place
    uint16_t addr;
    uint32_t ts; //tx timestamp
    uint32_t token;
    uint8_t rxCnt; //received counter together with adjecent ok message
}t_txTokPkt; //static struct array for logging tx packets info

/*packet information structs*/
 typedef struct{
	uint16_t addr;
	uint32_t token;
	uint8_t data[LTC_NCELLS];
	uint16_t pec;
	uint32_t ts; /* Cycle time when the message was "received" */
}t_rxCvPkt; //used by rx msg queue

/*intermediate timings- timestamps*/
typedef struct{
	uint32_t ok;
	uint32_t tok;
	uint32_t cv;
	uint32_t ack;
}t_stage;

/*latencies between crucial transaction steps*/
typedef struct{
	uint32_t cycleTime; //okAck
	uint32_t cycleStart; //timestamp Ok
	t_stage  intervals;
}t_proc;

/*collection of most recent timestamps-used by health mon.*/
typedef struct{
	uint32_t joinCmbTs;
	uint32_t lostCmbTs;
	uint32_t lastRecovery;
	uint32_t lastReconnect;
}t_ts;

/*information aggregated by bms about the overall communication quality that serves for safety monitoring*/
typedef struct {
	t_ts last;
	uint32_t lastUpdated; //used by radon
	uint32_t lastReceivedOk; //Used by reconnection
	uint8_t isActive;//Used by reconnection
	t_errCnt log; //comprehensive log for recent err/wrn messages
}t_diag; //static struct array for logging diagnostics of received packets
//=========================================================================

typedef struct {
	uint16_t addr;
	uint32_t addrToken;
	t_diag diag;
} hgNetClient;

//comprehensive structs with all the essential info

typedef struct {
	hgNetClient client[HG_NET_MAXCLIENTS];
	t_set info;
	t_health hm;
} hgNetState;

#elif NODETYPE == NODETYPE_CMB

/*create special message time to send this condensed diag info to BMS */
struct diagSum{
      uint32_t maxProcT; //processing time between token rx and cv tx
      uint32_t avgProcT;
      uint32_t runtime;
      uint32_t CvTxNum; //number of sent Cv pkts
};

typedef struct{
	uint32_t lastTxOk;
	uint32_t lastTxCv;
	uint32_t lastRxTok;
	uint32_t lastRxAck;
	uint32_t joinedTs; //when the node was joined to the net
}t_ts;

/*pkt information structs*/
 typedef struct{
	uint32_t token;
	uint32_t ts;	/* Cycle time when the message was "received" */
}t_rx; //used by rx msg queue

typedef struct{
      uint8_t * cv; //array with 8 bit cell values
      uint32_t token;
      uint16_t crc;
      uint32_t ts;    //TX timestamp
}t_tx;

typedef struct{
      t_ts last;
      uint8_t tx; //counters
      uint8_t rx;
      t_tx txmsg[LOG_ARR];
      t_rx rxmsg[LOG_ARR];
}t_logCmb;

typedef struct {
	uint16_t addr;
	uint32_t addrToken;
} hgNetClient;

#endif


#if NODETYPE == NODETYPE_BMS

void mercury_netMgr_InitState();
hgNetClient *mercury_netMgr_NewClient();

void ble_rxqueue_info();

void printConfigSt();
void sessionInfo();
void initConfigSt();

void printMeshHealth();

int message_dev_fetchRX(t_rxCvPkt *m);
void message_dev_pushQRX(t_rxCvPkt *m);

int message_dev_fetchTX(t_txTokPkt *m);
void message_dev_pushQTX(t_txTokPkt *m);

int message_dev_searchTX(t_rxCvPkt * rxmsg, t_txTokPkt *matched);

void mercury_netMgr_transactionUpdate(uint8_t nodeAddr, t_transS type);
void mercury_netMgr_packetCountUpdate(uint8_t nodeAddr,t_pktS type);
void mercury_netMgr_errorCountUpdate(uint8_t nodeAddr,t_nodeS type);
uint8_t mercury_netMgr_statesUpdate(uint8_t nodeAddr, t_pktS curr);
void mercury_netMgr_updateNodeState(uint16_t addr, t_nodeS state);
void mercury_netMgr_updateSystemState(t_sysSS state);
void mercury_netMgr_logWriteTime(uint8_t nodeAddr,t_pktS stage);

void mercury_netMgr_timeoutInit();
void mercury_netMgr_startDataTimer(uint8_t nodeIdx);
void mercury_netMgr_stopDataTimer(uint8_t nodeAddr);
void mercury_netMgr_startOkTimer(uint8_t nodeIdx);
void mercury_netMgr_stopOkTimer(uint8_t nodeAddr);

int mercury_netMgr_timeoutCheck();
void mercury_netMgr_recovery(int nodeIdx);

void mercury_mesh_forming();
void mercury_mesh_checkActiveNodes();
void mercury_mesh_setNodeInactive(uint8_t nodeAddr);

 typedef uint8_t t_qid; //see queue element identifiers 1-6

struct t_lat{
    void *fifo_reserved;
    uint8_t nid;
    t_proc timings;
};


#elif NODETYPE == NODETYPE_CMB
void mercury_netMgr_sendOK();

int message_dev_fetchRX(t_rx *m);
void message_dev_pushQRX(t_rx *m);

int message_dev_fetchTX(t_tx *m);
void message_dev_pushQTX(t_tx *m);
#endif

#endif
