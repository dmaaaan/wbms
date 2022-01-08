#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/model_types.h>
#include <net/net_pkt.h>
#include <random/rand32.h>
#include <stdio.h>

#include "buildcfg.h"
#include "led.h"
#include "mesh_proto.h"
#include "ltc_cmd.h"
#include "hg_netMgr.h"

#define DEBUG_INFO 0


static const uint8_t net_key[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
};
static const uint8_t dev_key[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
};
static const uint8_t app_key[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
};

static const uint16_t net_idx;
static const uint16_t app_idx;
static const uint32_t iv_index;
static uint8_t		  flags;

#if NODETYPE == NODETYPE_CMB
static uint16_t hwAddr = NODE_CMB_DEFADDR;
static uint16_t meshAddr = NODE_CMB_DEFADDR;
static uint16_t bmsAddr = NODE_BMS_ADDR;

#elif NODETYPE == NODETYPE_BMS
static uint16_t hwAddr = NODE_BMS_ADDR;
static uint16_t meshAddr = NODE_BMS_ADDR;
#endif

static const uint8_t dev_uuid[16] = { 0xdd, 0xdd };

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
};

uint16_t mercury_mesh_getHwAddr();
uint16_t mercury_mesh_getMeshAddr();
uint16_t mercury_mesh_getBmsAddr();

void mercury_mesh_init();

void mercury_mesh_updateAddr(uint16_t newMeshAddress, uint16_t newHwAddress);
void mercury_mesh_requestAddress();
void mercury_mesh_sendStart();
void mercury_mesh_msg_init();
void mercury_mesh_sendOK();
void mercury_mesh_recoverNodes(int nodeIdx);

#if NODETYPE == NODETYPE_BMS
uint8_t updateOKInfo(struct net_buf_simple * buf, uint16_t senderAddr);
uint8_t updateDataInfo(struct net_buf_simple * buf, uint16_t senderAddr);
void updateACKInfo(uint16_t dst);
void updateRecoverInfo(uint16_t dst);
void updateRejectedMsgInfo(uint16_t dst);
void updateTokenInfo(uint32_t token, uint16_t dst);
void mercury_mesh_resendACK(uint8_t nodeIdx);
void mercury_mesh_resendToken(uint8_t addr);
#elif NODETYPE == NODETYPE_CMB
void updateTokenInfo();
void updateOKInfo();

#endif

//void mercury_pingRX_handler(uint32_t cmbAddr, int pingTime);

