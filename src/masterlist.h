#ifndef LS2_SERVER_MASTERLIST_H
#define LS2_SERVER_MASTERLIST_H
#include <enet/enet.h>

#define MASTERLIST_MAX_PEERS 32

struct MasterState {
	ENetHost *host;

	uint16_t port;
	uint8_t  players;
	uint8_t  maxplayers;
	char     name[32];
	char     gamemode[8];
	char     map[21];

	/* Don't touch these or the devil will order you a gigaton of pizza
	 * to be dropped onto your house.
	 */
	char     oldmap[21];
	uint16_t oldport;
	uint8_t  oldplayers;
	uint8_t  oldmaxplayers;
	char     oldname[32];
	char     oldgamemode[8];

	void *udata;
	void (*on_reconnect_attempt)(uint32_t peer, void *udata);
	void (*on_successful_connect)(uint32_t peer, void *udata);
	void (*on_disconnect)(uint32_t peer, void *udata);
};

int masterlist_init(struct MasterState *ms);
void masterlist_deinit(struct MasterState *ms);

uint32_t masterlist_connect(ENetAddress *addr, struct MasterState *ms);
void masterlist_disconnect(uint32_t peer, struct MasterState *ms);
void masterlist_disconnect_all(struct MasterState *ms);

void masterlist_service(struct MasterState *ms);
#endif
