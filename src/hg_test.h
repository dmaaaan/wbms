#ifndef HG_TEST_H
#define HG_TEST_H

#include "hg_netMgr.h"

void print_summary();

//queue element identifiers
#define TIMING_CYCLIC 1	  //t_proc- 6x 2B
#define CNT_REGULAR 2	 //T_transCnt, t_pktCnt, t_errCnt- 10x 2B (3-transaction, 4- pktcnt, 4- errcnt) e.g. every next 50 tClosed
#define CONFIG_ONCE 3	//T_set: 2 x 4B t_misc + 3 x 4B + 3 x 1B t_sysConf
#define NODE_INFO_ONCE 4
#define BMS_MSG_SPORADIC 5	//Err_msg, wrn_msg

#define TIMING_CYCLIC_SIZE  17 //5 transmissions
#define CNT_REGULAR_SIZE 10 // 2.5 transmissions- 2 more or 2 less bytes if possible    
#define CONFIG_ONCE_SIZE 8
#define NODE_INFO_ONCE_SIZE 4 // 1 transmission
#define BMS_MSG_SPORADIC_SIZE 4 // 1 trans

/*all the functions interacting with Radon*/
void message_radon_init();
void radon_latenciesInfoTX();
void radon_configInfoTX();
void radon_countersInfoTX();

extern struct k_fifo hil_txqueue;

#endif