/* masterlist.c -- Implementation of a version 0.75 masterlist client */
#include "masterlist.h"
#include <string.h>

static int broadcast_packet(const void *data, size_t len, struct MasterState *ms) {
	ENetPacket *packet;

	packet = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
	if (packet == NULL)
		return -1;

	enet_host_broadcast(ms->host, 0, packet);
	return 0;
}

static int send_packet(const void *data, size_t len, ENetPeer *peer) {
	ENetPacket *packet;

	packet = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
	if (packet == NULL)
		return -1;

	if (enet_peer_send(peer, 0, packet) < 0)
		return -1;

	return 0;
}

#define ADD_STR(str) do { \
	size_t len; \
	len = strlen(str) + 1; \
	memcpy(buf+off, str, len); \
	off += len; \
} while (0)

static size_t major_buf(char *buf, struct MasterState *ms) {
	size_t off = 0;
	
	buf[off++] = ms->maxplayers;

	memcpy(buf+off, &ms->port, 2);
	off += 2;

	ADD_STR(ms->name);
	ADD_STR(ms->gamemode);
	ADD_STR(ms->map);

	return off;
}

static void minor(struct MasterState *ms) {
	broadcast_packet(&ms->players, 1, ms);
}

static void major(struct MasterState *ms) {
	char buf[1+2+32+8+21];
	broadcast_packet(buf, major_buf(buf, ms), ms);

	if (ms->players != 0)
		minor(ms);
}

static void on_connect(ENetPeer *peer, struct MasterState *ms) {
	char buf[1+2+32+8+21];
	send_packet(buf, major_buf(buf, ms), peer);

	if (ms->players != 0)
		send_packet(&ms->players, 1, peer);
}

int masterlist_init(struct MasterState *ms) {
	memset(ms, 0, sizeof(*ms));

	/* TODO: should incomingBandwidth be set to something like 1 */
	/* TODO: unlimit peers, somehow */
	ms->host = enet_host_create(NULL, MASTERLIST_MAX_PEERS, 1, 0, 0);
	if (ms->host == NULL)
		return -1;

	if (enet_host_compress_with_range_coder(ms->host) != 0) {
		enet_host_destroy(ms->host);
		return -1;
	}

	return 0;
}

void masterlist_deinit(struct MasterState *ms) {
	enet_host_destroy(ms->host);
}

/* Some random pointer that has a non-NULL value */
#define AUTORECONNECT masterlist_connect

#include <stdio.h>
#define DBGLOG(x, ...) do {fprintf(stderr, x"\n\r", __VA_ARGS__);} while (0)
uint32_t masterlist_connect(ENetAddress *addr, struct MasterState *ms) {
	ENetPeer *peer;
	DBGLOG("ATTEMPT CONNECT", NULL);

	peer = enet_host_connect(ms->host, addr, 1, 31);
	if (peer == NULL)
		return (uint32_t)-1;
	peer->data = AUTORECONNECT;
	return peer->incomingPeerID;
}

void masterlist_disconnect(uint32_t peer, struct MasterState *ms) {
	/* TODO: disconnect_now or try setting peer, 1 instead of peer, 0? */
	enet_peer_disconnect(ms->host->peers+peer, 0);
	ms->host->peers[peer].data = NULL;
}

void masterlist_disconnect_all(struct MasterState *ms) {
	uint32_t i;
	/* TODO: disconnect non-complete connections */
	for (i=0;i<MASTERLIST_MAX_PEERS;i++) {
		if (ms->host->peers[i].state == ENET_PEER_STATE_CONNECTED)
			masterlist_disconnect(i, ms);
	}
}

static void update(struct MasterState *ms) {
	if ((ms->port != ms->oldport               && (ms->oldport = ms->port,1))                ||
	    (strcmp(ms->name, ms->oldname)         && (strcpy(ms->oldname, ms->name),1))         ||
	    (strcmp(ms->gamemode, ms->oldgamemode) && (strcpy(ms->oldgamemode, ms->gamemode),1)) ||
	    (strcmp(ms->map, ms->oldmap)           && (strcpy(ms->oldmap, ms->map),1))           ||
	    (ms->maxplayers != ms->oldmaxplayers   && (ms->oldmaxplayers = ms->maxplayers,1)))
		major(ms);
	else if (ms->players != ms->oldplayers && (ms->oldplayers = ms->players,1))
		minor(ms);
}

void masterlist_service(struct MasterState *ms) {
	ENetEvent event;

	update(ms);

	/* TODO: can i enable multichannel in enet without screwing things up? */
	/* TODO: should this be if and not while? or maybe use enet_host_check_events */
	while (enet_host_service(ms->host, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_CONNECT:
			DBGLOG("CONNECT", NULL);
			on_connect(event.peer, ms);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			DBGLOG("DISCONNECT", NULL);
			/* TODO: log */
			if (event.peer->data && event.data == 0) {
				event.peer->data = NULL;
				masterlist_connect(&event.peer->address, ms);
			}
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			DBGLOG("RECV?", NULL);
			/* This is never supposed to happen. */
			enet_packet_destroy(event.packet);
			enet_peer_reset(event.peer);
			break;
		case ENET_EVENT_TYPE_NONE:
			break;
		}
	}
}
