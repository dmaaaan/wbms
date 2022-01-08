#ifndef APP_SLAVE_H //define app_slave only once
#define APP_SLAVE_H

#include <zephyr.h>
#include <random/rand32.h>
#include <app_memory/app_memdomain.h>
#include <sys/libc-hooks.h>
#include <syscall_handler.h>
#include <sys/crc.h> //cmb

#define CMB_SLEEP_TIME 10
#define STACK_SIZE 1024
#define POLLING_INT 100

#define MAIN_THREAD_PRIO 0//-7
#define POLL_THREAD_PRIO 1

void mercury_wakeup_main_cmb();
void mercury_cmb_init();
void mercury_acquire_nodeAddress();

void mercury_main_cmb(void *p1, void *p2, void *p3);
void polling_fn(void *p1, void *p2, void *p3);

#endif
