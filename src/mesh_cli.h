#include <bluetooth/mesh.h>
#include <bluetooth/mesh/model_types.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <random/rand32.h>
#include <stdio.h>
#include <string.h>

#include <zephyr.h>
#include <string.h>

#include "hg_netMgr.h"

#define MOD_LF			 0x0000
#define GROUP_ADDR		 0xc000
#define PUBLISHER_ADDR		  0x000f

#define UICR_Customer0	 *((volatile uint32_t *)(0x10001000 + 0x080))

#define OP_ADDR_RQ BT_MESH_MODEL_OP_3(0x10, BT_COMP_ID_LF)
#define OP_ADDR_RP BT_MESH_MODEL_OP_3(0x11, BT_COMP_ID_LF)

#define OP_DATA		BT_MESH_MODEL_OP_3(0x12,BT_COMP_ID_LF)
#define OP_ACK		BT_MESH_MODEL_OP_3(0x13,BT_COMP_ID_LF)
#define OP_START	BT_MESH_MODEL_OP_3(0x14,BT_COMP_ID_LF)
#define OP_OK		BT_MESH_MODEL_OP_3(0x15, BT_COMP_ID_LF)
#define OP_TOKEN	BT_MESH_MODEL_OP_3(0x16, BT_COMP_ID_LF)
#define OP_RCV		BT_MESH_MODEL_OP_3(0x17,BT_COMP_ID_LF)

//#define OP_VND_HEARTBEAT  BT_MESH_MODEL_OP_3(0x15, BT_COMP_ID_LF)

//#define BT_MESH_MODEL_OP_SENSOR_GET BT_MESH_MODEL_OP_2(0x82, 0x31)
//#define BT_MESH_MODEL_OP_SENSOR_STATUS BT_MESH_MODEL_OP_2(0x00, 0x52)

extern const struct bt_mesh_model_op vnd_ops[];


#if NODETYPE == NODETYPE_CMB
void bt_mesh_cli_sendOK(struct bt_mesh_model *model);
void bt_mesh_cli_request_address(struct bt_mesh_model *model);
#elif NODETYPE == NODETYPE_BMS
void bt_mesh_cli_allowStart(struct bt_mesh_model *model, uint16_t destAddr);
void bt_mesh_cli_sendRecover(struct bt_mesh_model *model, uint16_t destAddr);
void bt_mesh_cli_sendACK(struct bt_mesh_model *model, uint16_t destAddr);
void bt_mesh_cli_sendToken(struct bt_mesh_model *model, uint8_t addr);
void bt_mesh_cli_flushFifos(); 
#endif
//void bt_mesh_cli_send_var_size(struct bt_mesh_model *model, t_txmsg * pkt);


//own buffer definition- not used

//struct bt_mesh_me_cli {
//	/** Access model pointer. */
//	struct bt_mesh_model *model;
//	/** the rest is for public messages*/
//	/** Publish parameters. */
//	struct bt_mesh_model_pub pub;
//	/** Publication message. */
//	struct net_buf_simple pub_msg;
//	/** Publication message buffer. */
//	uint8_t buf[BT_MESH_MODEL_BUF_LEN(OP_DIR_MSG_RQ, BT_MESH_CLI_MSG_MAXLEN_MESSAGE)];
//	/** Handler function structure. */
//	//const struct bt_mesh_me_cli_handlers *handlers;
//	/** Current Presence value. */
//	//enum bt_mesh_me_cli_presence presence;
//};


//struct bt_mesh_me_mesh_cli;

//struct bt_mesh_me_mesh_cli_handlers {
//	void (*const message_reply)(struct bt_mesh_me_mesh_cli *me, struct bt_mesh_msg_ctx *ctx, const char* msg);
//};

//struct bt_mesh_me_mesh_cli {

//	struct bt_mesh_model *model;

//	const struct bt_mesh_me_mesh_cli_handlers *handler;
//};

//#if NODETYPE == NODETYPE_CMB
//extern const struct bt_mesh_model_op sensor_srv_op[];
//extern const struct bt_mesh_model_pub sensor_srv_pub;
//#endif
//#if NODETYPE == NODETYPE_BMS
//extern const struct bt_mesh_model_op sensor_cli_op[];
//#endif




