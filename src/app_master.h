#ifndef APP_MASTER_H //define app_master only once
#define APP_MASTER_H

#include <zephyr.h>
#include <app_memory/app_memdomain.h>
#include <sys/sys_heap.h>
#include <sys/crc.h>


#include <sys/libc-hooks.h>
#include <syscall_handler.h>

#include "hg_netMgr.h"

#define STACK_SIZE 1024
//TX PRIO- -9, RX PRIO- -8
//#define LISTEN_THREAD_PRIO (K_PRIO_COOP(CONFIG_BT_RX_PRIO)+1)//-6, cooperative priorities.Lower thread numbers have higher priority values, e.g. thread 0 will be preempted only by the listen thread
#define DIAG_THREAD_PRIO (K_PRIO_COOP(CONFIG_BT_RX_PRIO)+1)  //-7
#define MAIN_THREAD_PRIO (DIAG_THREAD_PRIO+1) //preemptive


/*
listen = -7 (highest coop here), diag= -6 
main = 1 

RX data -(resume listen)->Listen->(suspend listen)->Diag-(diag sleep)-> main-(main sleep)->Diag..
currently:
8 diags / 1 main

*/

#define DIAG_SLEEP_TIME 10
#define MAIN_SLEEP_TIME 5
#define PRINT_PERIOD 5000

//void listen_fn(void *p1, void *p2, void *p3);
void mercury_main_bms(void *p1, void *p2, void *p3) ;
void diag_fn(void *p1, void *p2, void *p3);

//void mercury_resume_listen();
void mercury_bms_init();

#endif