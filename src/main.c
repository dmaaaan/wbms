#include "buildcfg.h"
#include "main.h"
#include "led.h"
#include "hg_mesh.h"
#include "hg_netMgr.h"
#if NODETYPE == NODETYPE_BMS
#include "app_master.h"
#elif NODETYPE == NODETYPE_CMB
#include "app_slave.h"
#endif

#ifndef NODETYPE
#error NODETYPE undefined, unable to generate a valid image
#endif

extern void mercury_preload_RXLine(char *dest, uint8_t *rSize, uint16_t timeout);
extern void mercury_preload_disableMPU(void);

extern void mercury_fwupd_entryPoint(void);
extern void mercury_fwupd_stageF(void);

#if NODETYPE == NODETYPE_BMS

//struct k_thread listen_thread;
//K_THREAD_STACK_DEFINE(listen_stack, STACK_SIZE);

struct k_thread diag_thread;
K_THREAD_STACK_DEFINE(diag_stack, STACK_SIZE);

struct k_thread main_thread;
K_THREAD_STACK_DEFINE(main_stack, STACK_SIZE);

#elif  NODETYPE == NODETYPE_CMB

struct k_thread main_thread;
K_THREAD_STACK_DEFINE(main_stack, STACK_SIZE);

struct k_thread polling_thread;
K_THREAD_STACK_DEFINE(polling_stack, STACK_SIZE);

#endif


void main(void)
{
	//Disable any special memory protection stuff that might impede fwupd
	mercury_preload_disableMPU();

	//Initialise the mercury LED stuff so we can do blinky
	mercury_led_init();
	mercury_led_flash(10);

	printk("MErcury system startup\n");

	//Firmware update detect
	{
		uint32_t sTime = k_cycle_get_32();
		char	 buffer[64];
		uint8_t	 dSize = 0;

		//Wait for 15 seconds to see if anything comes in over UART
		while (k_cycle_get_32() - sTime < 15 * 1000) {
			mercury_preload_RXLine(buffer, &dSize, 100);

			//Something came in over UART, lets do things!
			if (dSize > 0) {
				//Should we expect firmware data to follow?
				if (strncmp(buffer, "fwupd", 5) == 0) {
					mercury_fwupd_entryPoint();
				}
			}
		}
	}
	
	#if NODETYPE == NODETYPE_BMS
	mercury_bms_init();
	initConfigSt(); //not used in anyway yet. just writing all config to struct
	#elif NODETYPE == NODETYPE_CMB
	mercury_cmb_init();
	#endif

	//Bring up the mesh
	mercury_mesh_init();

	#if NODETYPE == NODETYPE_CMB
	mercury_acquire_nodeAddress();
	#endif

	#if NODETYPE == NODETYPE_BMS
	mercury_mesh_forming();
	

	k_thread_create(&main_thread, main_stack, STACK_SIZE,
	mercury_main_bms, NULL, NULL, NULL,
	MAIN_THREAD_PRIO, K_INHERIT_PERMS, K_NO_WAIT);

	k_thread_create(&diag_thread, diag_stack, STACK_SIZE,
	diag_fn, NULL, NULL, NULL,
	DIAG_THREAD_PRIO, K_INHERIT_PERMS, K_NO_WAIT);

#elif NODETYPE == NODETYPE_CMB
	k_thread_create(&polling_thread, polling_stack, STACK_SIZE,
	polling_fn, NULL, NULL, NULL,
	POLL_THREAD_PRIO, K_INHERIT_PERMS, K_NO_WAIT);

	k_thread_create(&main_thread, main_stack, STACK_SIZE,
	mercury_main_cmb, NULL, NULL, NULL,
	MAIN_THREAD_PRIO, K_INHERIT_PERMS, K_MSEC(50));

	#endif
}