typedef struct {
	uint8_t data[16];
} hgMesh_pkt_Buffer;

typedef struct {
	uint8_t pktType;
	uint8_t data[15];
} hgMesh_pkt_Generic;

typedef struct {
	uint8_t	 pktType;
	uint8_t	 opCode;
	uint32_t uuid;
} hgMesh_pkt_NodeAddrReq;

typedef struct {
	uint8_t	 pktType;
	uint8_t	 opCode;
	uint32_t uuid;
	uint32_t address;
} hgMesh_pkt_NodeAddrRepl;
