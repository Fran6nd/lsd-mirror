#include <time.h>
#include <stdio.h>
#include <math.h>
#include <isa-l.h>
#include "state.h"
#include "pvx/src/vxl.h"
#include "budgetvxl.h"
#include "cull.h"
#include "demoncore.h"
#include "sandbox.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

/* TODO: can i no-op a map load for clients that won't take direct statedata? */
/* TODO: what happens if a smelly haxor takes the intel out of bounds (in pyspades)?
 * well there's some nice duplicated code, one in player.py and one in gamemodes.py for dropping intel
 * not sure the gamemodes.py one is ever called
 * tc has drop_flag
 */

/* Tick rate in Hz. Every tick physics and such is calculated. */
#define TICKRATE 60
//#define TICKRATE 120

ENetHost *bringup_host(ENetAddress *addr) {
	ENetHost *host;

	/* +1 to allow the 33st player connecting (assuming max is 32) to be sent a disconnect with "server full" as reason */
	host = enet_host_create(addr, MAX_PLAYERS+1, 1, 0, 0);

	if (host == NULL)
		return NULL;

	enet_host_compress_with_range_coder(host);

	return host;
}

/* TODO: should i specify an epoch? */
clk get_time(void) {
	struct timespec timeSpec;

	clock_gettime(CLOCK_MONOTONIC, &timeSpec);

	return timeSpec.tv_sec * 1000000000 + timeSpec.tv_nsec;
}

int time_until(clk ts) {
	clk now = get_time();

	if (ts > now)
		return ts-now;

	return 0;
}

clk to_ms(clk ts) {
	return ts / 1000000;
}

clk to_s(clk ts) {
	return ts / 1000000000;
}

double to_s_double(clk ts) {
	return (double)ts / 1000000000;
}

clk from_s(clk ts) {
	return ts * 1000000000;
}

clk from_s_double(double ts) {
	return ts * 1000000000;
}

void handle_event(ENetEvent *event, struct State *st) {
	switch (event->type) {
	case ENET_EVENT_TYPE_CONNECT:
		st->f.on_any_connect(event->peer->incomingPeerID, st);
		break;
	case ENET_EVENT_TYPE_DISCONNECT:
		st->f.on_disconnect(event->peer->incomingPeerID, st);
		break;
	case ENET_EVENT_TYPE_RECEIVE: {
		if (st->f.on_any_packet(event->peer->incomingPeerID, event->packet, st))
			st->f.on_crap_packet(event->peer->incomingPeerID, event->packet, st);
		else
			st->f.on_sane_packet(event->peer->incomingPeerID, event->packet, st);
		} break;
	case ENET_EVENT_TYPE_NONE:
		break;
	}
}

void do_loop(struct State *st) {
	ENetEvent event;

	while (enet_host_service(st->host, &event, to_ms(time_until(st->nextTickTime))) > 0) {
		handle_event(&event, st);
	}

	st->nextTickTime += st->tickrate;
	st->f.tick(st);
}

/* Use this for iterating over BROADCAST_* pids */
int pid_matches(plid broadcast, plid pid, struct State *st) {
	uint32_t flags = (uint32_t)broadcast >> 29;
	uint32_t data = (uint32_t)broadcast & (uint32_t)0x1fffffff;

	switch (flags) {
	case 1:
		/* PID_BROADCAST */
		return st->host->peers[pid].state == ENET_PEER_STATE_CONNECTED;
	case 2:
		/* PID_BROADCAST_EXCEPT */
		return (st->host->peers[pid].state == ENET_PEER_STATE_CONNECTED && pid != data);
	case 3:
		/* PID_BROADCAST_TEAM */
		return (st->p[pid].joined && st->p[pid].team == data);
	case 4:
		/* PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER */
		return (pid != data >> 8 && st->host->peers[pid].state == ENET_PEER_STATE_CONNECTED && (!st->p[pid].joined || (st->p[pid].joined && st->p[pid].team != (data & 0xff))));
	default:
		return broadcast == pid;
	}
}

int send_packet_flags(plid pid, const void *data, size_t length, unsigned flags, struct State *st) {
	ENetPacket *packet;

	packet = enet_packet_create(data, length, flags);
	if (packet == NULL)
		return -1;

	if (pid == PID_BROADCAST)
		enet_host_broadcast(st->host, 0, packet);
	else if ((uint32_t)pid < MAX_PLAYERS)
		return enet_peer_send(st->host->peers+pid, 0, packet) == 0 ? 0 : -1;
	else {
		plid i;
		for (i=0;i<MAX_PLAYERS;i++) {
			if (pid_matches(pid, i, st))
				enet_peer_send(st->host->peers+i, 0, packet);
		}
	}

	return 0;
}

int send_packet(plid pid, const void *data, size_t length, struct State *st) {
	return send_packet_flags(pid, data, length, ENET_PACKET_FLAG_RELIABLE, st);
}

int send_packet_unreliable(plid pid, const void *data, size_t length, struct State *st) {
	return send_packet_flags(pid, data, length, 0, st);
}

#define SEND(pid, data) st->f.send_packet(pid, &(data), sizeof(data), st)
#define LOG(x, ...) fprintf(stderr, x"\n", __VA_ARGS__)
#define LOG1(x) fputs(x"\n", stderr)

void send_fog(plid pid, color color, struct State *st) {
	struct PacketFogColor cf;

	cf.packetID = PacketTypeFogColor;
	cf.a = 0;
	cf.color[0] = color[0];
	cf.color[1] = color[1];
	cf.color[2] = color[2];

	SEND(pid, cf);
}

void set_fog(color color, struct State *st) {
	st->globals.fog[0] = color[0];
	st->globals.fog[1] = color[1];
	st->globals.fog[2] = color[2];

	st->f.send_fog(PID_BROADCAST, color, st);
}

/* TODO: what if i'm the last player ID? i don't need my own position */
void send_player_update(plid pid, struct State *st) {
	struct PacketWorldUpdate upd;
	plid i, max = -1;

	upd.packetID = PacketTypeWorldUpdate;

	memset(upd.players, 0, sizeof(upd.players));

	for (i=0;i<MAX_PLAYERS;i++) {
		/* TODO: do i care about the position of dead people? spectators? */
		if (!st->p[i].alive)
			continue;

		max = i;

		upd.players[i].pos = st->p[i].pos;
		upd.players[i].ori = st->p[i].ori;
	}

	st->f.send_packet_unreliable(pid, &upd, 1+(max+1)*24, st);
}

int alloc_more_nades(struct State *st) {
	struct Grenade *newbuf;
	size_t i;

	newbuf = calloc(st->globals.grenadeSize<<2, sizeof(struct Grenade));
	if (newbuf == NULL)
		return -1;

	memcpy(newbuf, st->globals.grenades, st->globals.grenadeSize*sizeof(struct Grenade));

	st->globals.grenadeSize <<= 2;
	st->globals.grenades = newbuf;
	return 0;
}

/* TODO: it might be a little major bit more efficient to fill in nades starting at the start, not the end */
int alloc_less_nades(struct State *st) {
	struct Grenade *newbuf;
	size_t smallSize = st->globals.grenadeSize;

	if (st->globals.grenadeSize == 256)
		return 0;

	while (smallSize > st->globals.grenadeCount)
		smallSize >>= 2;

	smallSize <<= 2;

	if (smallSize < 256)
		smallSize = 256;

	newbuf = realloc(st->globals.grenades, smallSize*sizeof(struct Grenade));
	if (newbuf == NULL)
		return -1;

	st->globals.grenadeSize = smallSize;
	st->globals.grenades = newbuf;
	return 0;
}

void remove_grenade(size_t index, struct State *st) {
	st->globals.grenades[index].exists = 0;

	while (st->globals.grenadeCount > 0 && !st->globals.grenades[st->globals.grenadeCount - 1].exists)
		st->globals.grenadeCount--;

	if (st->globals.grenadeCount < st->globals.grenadeSize && alloc_less_nades(st) == -1)
		LOG("realloc doesn't want to shrink the grenades buffer (%lu currently allocated)", st->globals.grenadeSize);
}

float sqr_len2(fvec3 vec) {
	return vec.x*vec.x + vec.y*vec.y;
}

float sqr_dist2(fvec3 pos1, fvec3 pos2) {
	pos1.x -= pos2.x;
	pos1.y -= pos2.y;

	return sqr_len2(pos1);
}

float sqr_len3(fvec3 vec) {
	return vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
}

float sqr_dist3(fvec3 pos1, fvec3 pos2) {
	pos1.x -= pos2.x;
	pos1.y -= pos2.y;
	pos1.z -= pos2.z;

	return sqr_len3(pos1);
}

float dist1(float p1, float p2) {
	return fabsf(p1 - p2);
}

float safe_sqr_dist3(fvec3 pos1, fvec3 pos2) {
	float val = sqr_dist3(pos1, pos2);

	if (val == 0)
		return 1;

	return val;
}

int libspades_voxel_bounds_check_map(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z) {
	return (x < MAP_SIZE_X && y < MAP_SIZE_Y && z < MAP_SIZE_Z);
}

int libspades_voxel_bounds_check_player(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z) {
	return (x < MAP_SIZE_X && y < MAP_SIZE_Y && z < (MAP_SIZE_Z - 2));
}

uint32_t *stackData;
uint64_t *rememberedSolidity;

static void destroyGrenadeVoxel(uint_fast32_t x,
                                uint_fast32_t y,
                                uint_fast32_t z,
                                int_fast8_t xOffset,
                                int_fast8_t yOffset,
                                int_fast8_t zOffset,
                                struct State *st) {
	if (!libspades_voxel_bounds_check_player(x + xOffset, y + yOffset, z + zOffset) ||
	    !pvx_voxel_get_solidity4(st->globals.map.solidData, CALC_I(x + xOffset, y + yOffset), z + zOffset))
		return;

	pvx_voxel_destroy4(st->globals.map.solidData, CALC_I(x + xOffset, y + yOffset), z + zOffset);
	if (xOffset != 0 && yOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y + yOffset, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		cull_floating_voxels(x + xOffset, y + yOffset * 2, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		cull_floating_voxels(x + xOffset, y + yOffset, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (xOffset != 0 && yOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y + yOffset, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		cull_floating_voxels(x + xOffset, y + yOffset * 2, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (xOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		cull_floating_voxels(x + xOffset, y, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (yOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x, y + yOffset * 2, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		cull_floating_voxels(x, y + yOffset, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (xOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (yOffset != 0) {
		cull_floating_voxels(x, y + yOffset * 2, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
	if (zOffset != 0) {
		cull_floating_voxels(x, y, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
		return;
	}
}

void libspades_grenade_destroy(uint_fast32_t x,
                               uint_fast32_t y,
                               uint_fast32_t z,
	                       struct State *st) {
	int_fast8_t xOffset, yOffset, zOffset;

	/* TODO: move bounds checking to each of these for loops? or is that terrible */
	for (zOffset = -1; zOffset <= 1; zOffset++)
		for (yOffset = -1; yOffset <= 1; yOffset++)
			for (xOffset = -1; xOffset <= 1; xOffset++)
				destroyGrenadeVoxel(x, y, z, xOffset, yOffset, zOffset, st);
}

void set_solid(ivec3 pos, struct State *st) {
	pvx_voxel_create4(st->globals.map.solidData, CALC_I(pos.x, pos.y), pos.z);
}

void set_vox_color(ivec3 pos, color clr, struct State *st) {
	pvx_voxel_color5(st->globals.map.colorData, clr, CALC_I(pos.x, pos.y), pos.z);
}

void set_empty3(int32_t x, int32_t y, int32_t z, struct State *st) {
	pvx_voxel_destroy4(st->globals.map.solidData, CALC_I(x, y), z);
}

void set_empty(ivec3 pos, struct State *st) {
	set_empty3(pos.x, pos.y, pos.z, st);
}

void cull3(int32_t x, int32_t y, int32_t z, struct State *st) {
	cull_floating_voxels(x, y, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity);
}

void cull(ivec3 pos, struct State *st) {
	cull3(pos.x, pos.y, pos.z, st);
}

void block_action(ivec3 pos, unsigned type, plid from, struct State *st) {
	switch (type) {
	case 0: /* build */
		set_solid(pos, st);
		set_vox_color(pos, st->p[from].blockColor, st);
		break;
	case 1: /* destroy */
		set_empty(pos, st);
		cull3(pos.x-1, pos.y, pos.z, st);
		cull3(pos.x+1, pos.y, pos.z, st);
		cull3(pos.x, pos.y-1, pos.z, st);
		cull3(pos.x, pos.y+1, pos.z, st);
		cull3(pos.x, pos.y, pos.z-1, st);
		cull3(pos.x, pos.y, pos.z+1, st);
		break;
	case 2: /* 3x destroy */
		if (pos.z > 0) {
			set_empty3(pos.x, pos.y, pos.z-1, st);
			cull3(pos.x-1, pos.y, pos.z-1, st);
			cull3(pos.x+1, pos.y, pos.z-1, st);
			cull3(pos.x, pos.y-1, pos.z-1, st);
			cull3(pos.x, pos.y+1, pos.z-1, st);
			cull3(pos.x, pos.y, pos.z-2, st);
		}

		set_empty3(pos.x, pos.y, pos.z, st);
		cull3(pos.x-1, pos.y, pos.z, st);
		cull3(pos.x+1, pos.y, pos.z, st);
		cull3(pos.x, pos.y-1, pos.z, st);
		cull3(pos.x, pos.y+1, pos.z, st);

		if (pos.z < 61) {
			set_empty3(pos.x, pos.y, pos.z+1, st);
			cull3(pos.x-1, pos.y, pos.z+1, st);
			cull3(pos.x+1, pos.y, pos.z+1, st);
			cull3(pos.x, pos.y-1, pos.z+1, st);
			cull3(pos.x, pos.y+1, pos.z+1, st);
			cull3(pos.x, pos.y, pos.z+2, st);
		}

		break;
	case 3: /* nade destroy */
		libspades_grenade_destroy(pos.x, pos.y, pos.z, st);
		break;
	}

	st->f.send_block_action(PID_BROADCAST, pos, type, from, st);
}

/* Fun fact: since distance is limited to 16 along each axis, the minimum grenade damage is 5. */
void detonate_grenade(size_t index, struct State *st) {
	plid i;
	ivec3 ipos;
	struct Grenade nade = st->globals.grenades[index];

	remove_grenade(index, st);

	/* TODO: what if player leaves? */
	/* Switching team and nading has potential to be annoying. */
	if (st->p[nade.pid].team != nade.team)
		return;

	/* TODO: cast2 -> pvx.so */
	for (i=0;i<MAX_PLAYERS;i++) {
		if (!st->p[i].alive || (st->p[i].team == nade.team && nade.pid != i))
			continue;

		if (dist1(st->p[i].pos.x, nade.pos.x) >= 16 ||
		    dist1(st->p[i].pos.y, nade.pos.y) >= 16 ||
		    dist1(st->p[i].pos.z, nade.pos.z) >= 16)
			continue;

		if (!cast_ray2(st->globals.map.solidData, nade.pos, st->p[i].pos)) {
			st->f.set_hp_directional(i, st->p[i].hp - 4096 / safe_sqr_dist3(nade.pos, st->p[i].pos), nade.pos, st);

			if (st->p[i].hp == 0)
				st->f.kill(i, KillTypeGrenade, nade.pid, st);
		}
	}

	ipos.x = floorf(nade.pos.x);
	ipos.y = floorf(nade.pos.y);
	ipos.z = floorf(nade.pos.z);

	st->f.block_action(ipos, BlockActionTypeGrenadeDestroy, 0, st);
}

void send_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, plid from, struct State *st) {
	struct PacketGrenade nade;

	nade.packetID = PacketTypeGrenade;
	nade.playerID = from;
	nade.fuseLength = fuse;
	nade.pos = pos;
	nade.vel = vel;

	SEND(pid, nade);
}

size_t register_grenade(plid pid, unsigned team, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	if (st->globals.grenadeCount == st->globals.grenadeSize && alloc_more_nades(st) == -1) {
		LOG("Out of memory for more grenades (%lu currently allocated)", st->globals.grenadeSize);
		return (size_t)-1;
	}

	st->globals.grenades[st->globals.grenadeCount].detonateTime = get_time()+from_s_double(fuse);
	st->globals.grenades[st->globals.grenadeCount].pos = pos;
	st->globals.grenades[st->globals.grenadeCount].vel = vel;
	st->globals.grenades[st->globals.grenadeCount].pid = pid;
	st->globals.grenades[st->globals.grenadeCount].team = team;
	st->globals.grenades[st->globals.grenadeCount++].exists = 1;

	return st->globals.grenadeCount-1;
}

size_t spawn_grenade(plid pid, unsigned team, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	size_t idx = st->f.register_grenade(pid, team, pos, vel, fuse, st);
	if (idx != (size_t)-1)
		st->f.send_grenade(PID_BROADCAST, pos, vel, fuse, 0, st);
	return idx;
}

/* TODO: make grenade, send_grenade funcs */
void on_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	st->f.register_grenade(pid, st->p[pid].team, pos, vel, fuse, st);
	st->f.send_grenade(PID_BROADCAST_EXCEPT(pid), pos, vel, fuse, 0, st);
}

void send_reload(plid pid, unsigned mag, unsigned reserve, plid from, struct State *st) {
	struct PacketWeaponReload rl;

	rl.packetID = PacketTypeWeaponReload;
	rl.playerID = from;
	rl.magazineAmmo = mag > 255 ? 255 : mag;
	rl.reserveAmmo = reserve > 255 ? 255 : reserve;

	SEND(pid, rl);
}

void on_reload(plid pid, unsigned mag, unsigned reserve, struct State *st) {
	st->f.send_reload(PID_BROADCAST_EXCEPT(pid), 255, 255, pid, st);
}

#if 1
void tick_player_physics(plid pid, float timeDelta, struct State *st) {
	move_player(st->p+pid, timeDelta, st->globals.map.solidData, 0); /* TODO: LOOP_PHYSICS (as a script?) */
}
#else
/* Experiment with physics looping */
void tick_player_physics(plid pid, float timeDelta, struct State *st) {
	/* TODO: better method of wrap detection? */
	/* TODO: just handle wrapping here, silly goose. . . */
	fvec3 oldpos = st->p[pid].pos;
	move_player(st->p+pid, timeDelta, st->globals.map.solidData, 1); /* TODO: LOOP_PHYSICS (as a script?) */
	if ((oldpos.x < 128 && st->p[pid].pos.x > 384) ||
	    (oldpos.x > 384 && st->p[pid].pos.x < 128) ||
	    (oldpos.y < 128 && st->p[pid].pos.y > 384) ||
	    (oldpos.y > 384 && st->p[pid].pos.y < 128))
		st->f.set_position(pid, st->p[pid].pos, st);
}
#endif

void tick(struct State *st) {
	size_t i;
	clk now = get_time();

	for (i=0;i<MAX_PLAYERS;i++) {
		if (!st->p[i].joined)
			continue;

		if (st->p[i].alive)
			st->f.tick_player_physics(i, (double)st->tickrate/1000000000, st);

		if (st->p[i].spawntime && get_time() > st->p[i].spawntime)
			st->f.spawn_player(i, st);
	}

	for (i=0;i<st->globals.grenadeCount;i++) {
		if (!st->globals.grenades[i].exists)
			continue;

		if (now >= st->globals.grenades[i].detonateTime) {
			st->f.detonate_grenade(i, st);
			continue;
		}

		move_grenade(st->globals.grenades+i, (double)st->tickrate/1000000000, st->globals.map.solidData, 1);
	}

	send_player_update(PID_BROADCAST, st);
}

const char *host_ip(ENetAddress *addr) {
	static char str[16];

	/* TODO: pass 16 or 15? */
	enet_address_get_host_ip(addr, str, 16);

	return str;
}
#define IP(pid) host_ip(&st->host->peers[pid].address)
#define PORT(pid) (st->host->peers[pid].address.port)

void on_any_connect(plid pid, struct State *st) {
	if (pid >= MAX_PLAYERS) {
		LOG("%s:%u (#%u) attempted to connect but server was full", IP(pid), PORT(pid), pid);
		/* TODO: should i disconnect_now or just disconnect? if just disconnect, should i increase the amount of connections? */
		enet_peer_disconnect_now(st->host->peers+pid, 4);
	} else
		st->f.on_successful_connect(pid, st);
}

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
ssize_t get_fd_size(int fildes) {
	struct stat sb;

	if (fstat(fildes, &sb) != 0)
		ERR("fstat");

	return sb.st_size;
}

void *map_file(const char *path, ssize_t *size) {
	int fd;
	void *mem;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		ERR("open");

	*size = get_fd_size(fd);

	mem = mmap(NULL, *size, PROT_READ, MAP_SHARED, fd, 0);

	if (mem == MAP_FAILED)
		ERR("mmap");

	close(fd);
	return mem;
}

void on_successful_connect(plid pid, struct State *st) {
	LOG("%s:%u (#%u) connected", IP(pid), PORT(pid), pid);

	st->f.send_map(pid, st);
}

void send_map_start(plid pid, unsigned size, struct State *st) {
	struct PacketMapStart ms;

	ms.packetID = PacketTypeMapStart;
	ms.mapSize = size;

	SEND(pid, ms);
}

#if 0
void send_map(plid pid, struct State *st) {
	uint8_t *chk;
	ssize_t mapsize;
	char *map;

	st->f.send_map_start(pid, 0, st);

	map = map_file("maps/map.vxl.zlib", &mapsize);

	chk = malloc(1+mapsize);
	chk[0] = PacketTypeMapChunk;
	memcpy(chk+1, map, mapsize);
	
	munmap(map, mapsize);

	st->f.send_packet(pid, chk, 1+mapsize, st);
	free(chk);

	st->f.send_state(pid, st);
}
#else

/* buf should be cols*65*4 bytes */
size_t get_vxl_chunk(void *buf, size_t coloff, size_t cols, struct State *st) {
	uint_fast32_t x, y;

	if (cols > 512*512-coloff)
		cols = 512*512-coloff;

	x = coloff % 512;
	y = coloff / 512;

	return pvx_dump_vxl(&st->globals.map, x, y, 512, 512, 64, buf, cols);
}

struct isal_zstream init_deflate(void) {
	struct isal_zstream stream;

	isal_deflate_init(&stream);

	stream.flush = NO_FLUSH;
	stream.gzip_flag = IGZIP_ZLIB;
	stream.end_of_stream = 0;
	stream.level = 2;
	stream.level_buf = malloc(ISAL_DEF_LVL2_DEFAULT);
	stream.level_buf_size = ISAL_DEF_LVL2_DEFAULT;

	return stream;
}

void send_compressed_map(plid pid, struct State *st) {
	struct isal_zstream stream;
	size_t cols, buflen;
	uint8_t vxlbuf[512*8*65*4];
	/* TODO: determine isa-l magic numbers */
	uint8_t outbuf[1+512*8*65*4+330];

	/* TODO NOTE: doesn't pyspades make a whole new copy of the map every time it wants to write something? efficiency. */
	outbuf[0] = PacketTypeMapChunk;

	stream = init_deflate();

	for (cols=0;cols<512*512;cols += 512*8) {
		buflen = get_vxl_chunk(vxlbuf, cols, 512*8, st);

		stream.next_in = vxlbuf;
		stream.avail_in = buflen;

		if (cols == 512*512-512*8)
			stream.end_of_stream = 1;

		do {
			stream.next_out = outbuf+1;
			stream.avail_out = 512*8*65*4+330;

			if (isal_deflate(&stream) != ISAL_DECOMP_OK) {
				LOG1("Some deflate err!");
				return;
			}

			st->f.send_packet(pid, outbuf, stream.next_out-outbuf, st);
		} while (stream.avail_in != 0);
	}

	free(stream.level_buf);
}

void send_map(plid pid, struct State *st) {
	size_t buflen;
	plid i;

	st->f.send_map_start(pid, 0, st);
	send_compressed_map(pid, st);

	/* TODO: should the iterator be moved to send_state? */
	for (i=0;i<MAX_PLAYERS;i++)
		if (pid_matches(pid, i, st))
			st->f.send_state(i, st);
}
#endif

static void fill_in_state(struct PacketStateData *sta, plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, struct State *st) {
	sta->packetID = PacketTypeStateData;
	sta->playerID = from;
	sta->fog[0] = fog[0];
	sta->fog[1] = fog[1];
	sta->fog[2] = fog[2];
	sta->teamcolor[0][0] = teamcolor[0][0];
	sta->teamcolor[0][1] = teamcolor[0][1];
	sta->teamcolor[0][2] = teamcolor[0][2];
	sta->teamcolor[1][0] = teamcolor[1][0];
	sta->teamcolor[1][1] = teamcolor[1][1];
	sta->teamcolor[1][2] = teamcolor[1][2];
	memset(sta->team1Name, 0, 20);
	strcpy(sta->team1Name, teamname[0]);
	strcpy(sta->team2Name, teamname[1]);
}

void send_state_ctf(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st) {
	struct PacketStateData sta;
	unsigned i;

	fill_in_state(&sta, pid, from, teamname, teamcolor, fog, st);
	sta.gamemode = 0;
	sta.gm.ctf.teamscore[0] = teamscore[0];
	sta.gm.ctf.teamscore[1] = teamscore[1];
	sta.gm.ctf.maxscore = maxscore;
	/* TODO: I like how the ordering is reversed from what you'd expect */
	/* TODO: why does openspades sometimes decide nobody is holding an intel */
	sta.gm.ctf.heldIntels = (holders[1] != -1) | ((holders[0] != -1) << 1);
	/* TODO: what happens if 255 holds an intel */
	for (i=0;i<2;i++) {
		if (holders[i] != -1) {
			memset(&sta.gm.ctf.intelloc[i], 0, sizeof(sta.gm.ctf.intelloc[i]));
			sta.gm.ctf.intelloc[i].playerID = holders[i];
		} else
			sta.gm.ctf.intelloc[i].position = intelpos[i];
	}
	sta.gm.ctf.tentpos[0] = tentpos[0];
	sta.gm.ctf.tentpos[1] = tentpos[1];

	st->f.send_packet(pid, &sta, 84, st);
}

void send_state_tc(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, unsigned tentcount, const fvec3 *tentpos, unsigned *tentteam, struct State *st) {
	struct PacketStateData sta;
	unsigned i;

	fill_in_state(&sta, pid, from, teamname, teamcolor, fog, st);
	sta.gamemode = 1;
	sta.gm.tc.territoryCount = tentcount;

	for (i=0;i<tentcount;i++) {
		sta.gm.tc.territories[i].pos = tentpos[i];
		sta.gm.tc.territories[i].team = tentteam[i];
	}

	/* TODO: we should probably validate tentcount from lua code */
	st->f.send_packet(pid, &sta, 33+tentcount, st);
}

void send_state(plid pid, struct State *st) {
	/* TODO: does this go before or after? */
	st->f.send_connected_players(pid, st);
	st->f.send_state_ctf(pid, pid, st->globals.teamname, st->globals.teamcolor, st->globals.fog, st->globals.teamscore, st->globals.maxscore, st->globals.intelplayers, st->globals.intelpos, st->globals.tentpos, st);
}

void send_connected_players(plid pid, struct State *st) {
	struct PacketExistingPlayer ep;
	struct PacketInput in;
	struct PacketWeaponInput wi;
	struct PacketKill ki;
	plid i;

	ep.packetID = PacketTypeExistingPlayer;
	in.packetID = PacketTypeInput;
	wi.packetID = PacketTypeWeaponInput;

	ki.packetID = PacketTypeKill;
	ki.killerID = 0;
	ki.killType = KillTypeFall;
	ki.respawnTime = 0;
	
	for (i=0;i<MAX_PLAYERS;i++) {
		if (!st->p[i].joined)
			continue;

		ep.playerID = i;
		ep.team = st->p[i].team;
		ep.weapon = st->p[i].weapon;
		ep.tool = st->p[i].tool;
		ep.score = st->p[i].score;
		ep.blue = st->p[i].blockColor[0];
		ep.green = st->p[i].blockColor[1];
		ep.red = st->p[i].blockColor[2];
		strcpy(ep.name, st->p[i].name);

		in.playerID = i;
		in.keyStates = st->p[i].inputs;

		wi.playerID = i;
		wi.weaponInput = st->p[i].mouseInputs;

		ki.playerID = i;

		st->f.send_packet(pid, &ep, 13+strlen(ep.name), st);

		if (in.keyStates != 0)
			SEND(pid, in);

		if (wi.weaponInput != 0)
			SEND(pid, wi);

		if (st->p[i].team != 255 && !st->p[i].alive)
			SEND(pid, ki);
	}
}

void on_disconnect(plid pid, struct State *st) {
	LOG("%s:%u (#%u) disconnected", IP(pid), PORT(pid), pid);

	if (st->p[pid].joined) {
		struct PacketPlayerLeft pl;

		pl.packetID = PacketTypePlayerLeft;
		pl.playerID = pid;

		SEND(PID_BROADCAST, pl);
	}

	st->p[pid].joined = 0;
	st->p[pid].alive = 0;

	st->f.after_player_destroy(pid, st);
}

#define CAT2(x,y) x##y
#define CAT(x,y) CAT2(x,y)

#define STR2(x) #x
#define STR(x) STR2(x)

#define BADRETURN do {st->crapline = __LINE__; return 1;} while (0)
#define SBAD(cond) do {if (cond) {st->crapcond = "SBAD("#cond");"; BADRETURN;}} while (0)
#define SCASEANY case CAT(PacketType, PCKT): st->crappacketname = STR(PCKT);
#define SCASEJOINED SCASEANY SBAD(!st->p[pid].joined);
#define SCASEALIVE SCASEANY SBAD(!st->p[pid].alive);
#define PCASE case CAT(PacketType, PCKT):
#define PACKET (*(struct CAT(Packet, PCKT) *)packet->data)
#define PACKETPTR ((struct CAT(Packet, PCKT) *)packet->data)
#define SRANGE(min, max) SBAD(packet->dataLength < (min) || packet->dataLength > (max))
#define SEXACT() SBAD(packet->dataLength != sizeof(struct CAT(Packet, PCKT)))
#define SNUL() SBAD(packet->data[packet->dataLength-1] != '\0')
#define SPID() SBAD(packet->data[1] != pid)

int get_solid3(int32_t x, int32_t y, int32_t z, struct State *st) {
	return pvx_voxel_get_solidity4(st->globals.map.solidData, CALC_I(x, y), z);
}

int get_solid(ivec3 pos, struct State *st) {
	return get_solid3(pos.x, pos.y, pos.z, st);
}

int neighboring_voxels(ivec3 pos, struct State *st) {
	int32_t off;
	int count = 0;

	for (off=-1;off<2;off+=2)
		count += ((uint32_t)(pos.x+off) < 512 && get_solid3(pos.x+off, pos.y, pos.z, st));

	for (off=-1;off<2;off+=2)
		count += ((uint32_t)(pos.y+off) < 512 && get_solid3(pos.x, pos.y+off, pos.z, st));

	for (off=-1;off<2;off+=2)
		count += ((uint32_t)(pos.z+off) < 64 && get_solid3(pos.x, pos.y, pos.z+off, st));

	return count;
}

#define SCLIP(xoff, yoff, zoff, vec) clip_player(vec.x + (xoff), vec.y + (yoff), vec.z + (zoff), st->globals.map.solidData, 0)
#define SCLIPB(zoff, vec) (SCLIP(-0.44, -0.44, zoff, vec) || SCLIP (-0.44, 0.44, zoff, vec) || SCLIP(0.44, -0.44, zoff, vec) || SCLIP(0.44, 0.44, zoff, vec))
int stuck_in_a_block(fvec3 pos, struct State *st) {
		return SCLIPB(1.34, pos) || SCLIPB(0.45, pos) || SCLIPB(-0.44, pos);
}

/* TODO: sometimes voxlap and rl trigger this on ori with <0.000001 */
#define CLOSE_ENOUGH_TO_1(x) (fabsf((x) - 1) < 0.00005)

#define HORIZONTAL_SPEED_LIMIT 10.4
#define DOWNWARD_SPEED_LIMIT 32.5403 /* Normally just 32, but sometimes OpenSpades likes to send more */
#define UPWARD_SPEED_LIMIT 11.52 /* TODO: why was this 13.52 before */ /* TODO: needs some tweaking */
#define COMBINED_SPEED_LIMIT 32.16

/* TODO: does spawning affect openspades' position send time? didn't i already mention this somewhere? */
/* TODO: horizontal speed limit inexplicably being screwed at 94-ish min with openspades (most seen: 115.501671) */
#define HORIZONTAL_SPEED_LIMIT_SQR 111.82 /* Nominally 108.16 */
#define COMBINED_SPEED_LIMIT_SQR 1069.46 /* Usually 1034.2656, except when it's not */

/* TODO: these ones don't account for positiondata timing fuckery -- document that outside of this todo! */
#define PLAYER_VEL_LIMIT 1.005
#define PLAYER_VEL_LIMIT_SQR 1.010025
#define PLAYER_HVEL_LIMIT_SQR 0.1056250
#define PLAYER_DVEL_LIMIT_SQR 1
#define PLAYER_UVEL_LIMIT_SQR 0.1296

#define NADE_VEL_LIMIT_SQR 4.020025
#define NADE_HVEL_LIMIT_SQR 1.755625
#define NADE_DVEL_LIMIT_SQR 4
#define NADE_UVEL_LIMIT_SQR 1.8496

#define NOT_THE_SAME_POSITION(p1, p2) (p1.x != p2.x || p1.y != p2.y || p1.z != p2.z)

int on_any_packet(plid pid, ENetPacket *packet, struct State *st) {
	st->crappacketname = "?";

	SBAD(packet->dataLength < 1);

	switch (packet->data[0]) {
#define PCKT PositionData
		SCASEALIVE
		SEXACT();

		/* TODO: remove debugging cruft? or embrace it? */
		if (PACKET.pos.z > 62.65)
			LOG("Z: %f", PACKET.pos.z);

		/* TODO: player can't be higher than a certain height without server intervention */
		/* TODO: where did this magic 62.65 number come from? */
		SBAD(PACKET.pos.x < 0.45 || PACKET.pos.x > 511.55);
		SBAD(PACKET.pos.y < 0.45 || PACKET.pos.y > 511.55);
		SBAD(PACKET.pos.z > 62.65);
		/* TODO: make it suck less */
		/* TODO: should it be last agreed or regular flavor? */
		/* TODO: was_ever_not_in_a_block_since_lastagreedpos heuristic? */
		SBAD(stuck_in_a_block(st->p[pid].lastagreedpos, st) && stuck_in_a_block(st->p[pid].pos, st) && stuck_in_a_block(PACKET.pos, st) && NOT_THE_SAME_POSITION(PACKET.pos, st->p[pid].pos));
		if (PACKET.pos.z - st->p[pid].lastagreedpos.z > DOWNWARD_SPEED_LIMIT) LOG("dist1: %f", PACKET.pos.z - st->p[pid].lastagreedpos.z);
		SBAD(PACKET.pos.z - st->p[pid].lastagreedpos.z > DOWNWARD_SPEED_LIMIT);
		SBAD(st->p[pid].lastagreedpos.z - PACKET.pos.z > UPWARD_SPEED_LIMIT);
		if (sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos) > HORIZONTAL_SPEED_LIMIT_SQR) LOG("dist2: %f", sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos));
		SBAD(sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos) > HORIZONTAL_SPEED_LIMIT_SQR);
		if (sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos) > COMBINED_SPEED_LIMIT_SQR) LOG("dist3: %f", sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos));
		SBAD(sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos) > COMBINED_SPEED_LIMIT_SQR);

		return 0;
#undef PCKT
#define PCKT OrientationData
		SCASEALIVE
		SEXACT();

		SBAD(!CLOSE_ENOUGH_TO_1(PACKET.ori.x*PACKET.ori.x + PACKET.ori.y*PACKET.ori.y + PACKET.ori.z*PACKET.ori.z));

		return 0;
#undef PCKT
#define PCKT SetColor
		SCASEALIVE
		SEXACT();
		SPID();

		return 0;
#undef PCKT
#define PCKT Input
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: keyStates -> keys */
		return 0;
#undef PCKT
#define PCKT WeaponInput
		SCASEALIVE
		SEXACT();
		SPID();

		return 0;
#undef PCKT
#define PCKT ChatMessage
		SCASEJOINED
		SRANGE(4, 4+255);
		SPID();
		SNUL();

		SBAD(PACKET.type > 1);

		return 0;
#undef PCKT
#define PCKT ExistingPlayer
		SCASEANY
		SRANGE(13, 28);
		/* OpenSpades doesn't bother with SPID(); */
		SNUL();

		SBAD(st->p[pid].joined && st->p[pid].team != 255);

		/* TODO: should a spectator be allowed to switch to spectator? this doesn't match shortplayer (RENAME: something better; SpectatorSwitch?) either */
		SBAD(PACKET.team > 1 && PACKET.team != 255);
		SBAD(PACKET.weapon > 2);
		SBAD(PACKET.tool != ToolTypeGun);
		/* OpenSpades puts its score in PACKET.score (or some other data; I didn't check) for some reason despite being ignored */
		/* blue, green and red are ignored */

		return 0;
#undef PCKT
#define PCKT ShortPlayerData
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(st->p[pid].team != 255);

		SBAD(PACKET.team > 1);
		SBAD(PACKET.weapon > 2);

		return 0;
#undef PCKT
#define PCKT ChangeTeam
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(st->p[pid].team == 255);

		SBAD(PACKET.team > 1 && PACKET.team != 255);

		return 0;
#undef PCKT
#define PCKT ChangeWeapon
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(st->p[pid].team == 255);

		SBAD(PACKET.weapon > 2);

		return 0;
#undef PCKT
#define PCKT Hit
		SCASEALIVE
		SEXACT();

		/* TODO: rename playerID here, it misleads -- playerID is not the player's ID, just the ID of the hit player */
		SBAD(PACKET.playerID > MAX_PLAYERS);
		SBAD(PACKET.playerID == pid);
		/* Remember that spectators are not alive. */
		SBAD(!st->p[PACKET.playerID].alive);

		/* TODO: didn't betterspades suck at this */
		SBAD(!(st->p[pid].mouseInputs & 1));

		SBAD(st->p[pid].tool != ToolTypeGun && st->p[pid].tool != ToolTypeSpade);

		/* TODO: witchcraft-based range validation */
		SBAD(st->p[pid].tool == ToolTypeGun && PACKET.type > 3);
		SBAD(st->p[pid].tool == ToolTypeSpade && PACKET.type != 4);
		
		return 0;
#undef PCKT
#define PCKT Grenade
		SCASEJOINED /* Dead men can throw nades (unless you're pyspades). */
		SEXACT();
		SPID();

		/* TODO: should this really be here? */
		SBAD(st->p[pid].grenades == 0);
		st->p[pid].grenades--;

		SBAD(st->p[pid].team == 255);

		/* TODO: openspades switches back earlier than it should */
		//SBAD(st->p[pid].tool != ToolTypeGrenade);

		/* TODO: there's some range slightly above 0 and slightly below 3 that is actually used */
		SBAD(PACKET.fuseLength < 0);
		SBAD(PACKET.fuseLength > 3);

		/* TODO: witchcraft position validation */
		SBAD(PACKET.pos.x <= 0 || PACKET.pos.x >= 512);
		SBAD(PACKET.pos.y <= 0 || PACKET.pos.y >= 512);
		SBAD(PACKET.pos.z >= 64);

		/* TODO: should i bother with finding the true up/down values? betterspades ignores them. . . */
		/* TODO: wonder if a fancily-oriented player throws fancily-velocitied nades */
		SBAD(sqr_len3(PACKET.vel) > NADE_VEL_LIMIT_SQR + 1);
		SBAD(sqr_len2(PACKET.vel) > NADE_HVEL_LIMIT_SQR + 1);
		SBAD(PACKET.vel.z > NADE_DVEL_LIMIT_SQR + 1);
		SBAD(-PACKET.vel.z > NADE_UVEL_LIMIT_SQR + 1);

		/* OpenSpades disagrees here */
		/* TODO: didn't betterspades also suck at this */
		//SBAD(!(st->p[pid].mouseInputs & 1));

		return 0;
#undef PCKT
#define PCKT BlockAction
		SCASEALIVE
		SEXACT();
		SPID();

		SBAD((uint32_t)PACKET.pos.x >= 512);
		SBAD((uint32_t)PACKET.pos.y >= 512);
		SBAD((uint32_t)PACKET.pos.z >= 62);

		/* TODO: voxlap block decrement/increment would go here -- just add a callback? */

		switch(st->p[pid].tool) {
		case ToolTypeSpade:
			SBAD(PACKET.type != 1 && PACKET.type != 2);
			break;
		case ToolTypeBlock:
			SBAD(PACKET.type != 0);
			break;
		case ToolTypeGun:
			SBAD(PACKET.type != 1);
			break;
		default:
			st->crapcond = "BlockAction with invalid tool (probably grenade)";
			BADRETURN;
		}

		/* TODO: needs hard-crap and soft-crap packets */
		switch (PACKET.type) {
		case 0: /* build */
			SBAD(get_solid(PACKET.pos, st));
			SBAD(neighboring_voxels(PACKET.pos, st) == 0);
			SBAD(st->p[pid].blocks == 0);
			break;
		case 1: /* destroy, destroy 3x */
		case 2:
			SBAD(!get_solid(PACKET.pos, st));
			//SBAD(neighboring_voxels(PACKET.pos, st) == 6);
			break;
		}

		return 0;
#undef PCKT
#define PCKT BlockLine
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: limit action/line/hit to farthest hypothetical position given last agreed position */

		SBAD((uint32_t)PACKET.start.x >= 512);
		SBAD((uint32_t)PACKET.start.y >= 512);
		SBAD((uint32_t)PACKET.start.z >= 62);

		SBAD((uint32_t)PACKET.end.x >= 512);
		SBAD((uint32_t)PACKET.end.y >= 512);
		SBAD((uint32_t)PACKET.end.z >= 62);

		SBAD(st->p[pid].tool != ToolTypeBlock);

		/* TODO: block lines still work when the start is solid, right? */
		SBAD(neighboring_voxels(PACKET.start, st) == 0);

		SBAD(get_solid(PACKET.end, st));
		SBAD(neighboring_voxels(PACKET.end, st) == 0);

		/* TODO: limit len to 50 */

		return 0;
#undef PCKT
#define PCKT SetTool
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: ammo, etc. */
		SBAD(PACKET.tool > 3);

		return 0;
#undef PCKT
#define PCKT WeaponReload
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: validate reserve ammo */

		return 0;
#undef PCKT
	}

	st->crapcond = "Unknown packet ID";
	BADRETURN;
}

void send_chat(plid pid, const char *msg, unsigned type, plid from, struct State *st) {
	size_t msglen = strlen(msg);
	uint8_t *chat = malloc(3 + msglen + 1);

	if (chat == NULL)
		ERR("malloc");

	chat[0] = PacketTypeChatMessage;
	chat[1] = from;
	chat[2] = type;
	memcpy(chat+3, msg, msglen+1);

	st->f.send_packet(pid, chat, 3 + msglen + 1, st);

	free(chat);
}

/* TODO: CP437, etc. . . */
void on_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	plid dest;

	LOG("(%s) %s: %s", type == ChatTypeAll ? "Global" : "Team", st->p[pid].name, msg);

	if (type == ChatTypeAll)
		dest = PID_BROADCAST;
	else
		dest = PID_BROADCAST_TEAM(st->p[pid].team);

	st->f.send_chat(dest, msg, type, pid, st);
}

void on_join(plid pid, unsigned team, unsigned weapon, const char *name, struct State *st) {
	LOG("%s:%u (#%u) joined as \"%s\"", IP(pid), PORT(pid), pid, name);

	/* at this point the player is still not alive */
	st->p[pid].joined = 1;
	st->p[pid].score = 0;
	st->p[pid].newteam = team;
	st->p[pid].newweapon = weapon;
	strcpy(st->p[pid].name, name);

	st->f.spawn_player(pid, st);
}

void on_switch(plid pid, unsigned team, unsigned weapon, struct State *st) {
	/* TODO: should its use as :kill be permitted? */
	//LOG("%s:%u (#%u) tried to switch or something", IP(pid), PORT(pid), pid);

	st->p[pid].newteam = team;
	st->p[pid].newweapon = weapon;

	/* TODO: should spectators haven't a respawn timer? */
	/* TODO: how to only switch after spawn? */
	if (st->p[pid].team == 255) {
		st->f.spawn_player(pid, st);
		if (st->p[pid].alive)
			st->f.kill(pid, KillTypeTeamChange, 0, st);
	} else if (team != st->p[pid].team && st->p[pid].alive)
		st->f.kill(pid, KillTypeTeamChange, 0, st);
	else if (st->p[pid].alive)
		st->f.kill(pid, KillTypeWeaponChange, 0, st);
}

int32_t highest_point(int32_t x, int32_t y, struct State *st) {
	int32_t z;

	for (z=0;z<64;z++) {
		if (get_solid3(x, y, z, st))
			return z;
	}

	return 64;
}

float highest_point_spawn(int32_t x, int32_t y, struct State *st) {
	float z;

	z = highest_point(x, y, st);
	if (z == 63)
		z = 64;

	z -= 2.251;
	
	return z;
}

fvec3 on_player_spawn(plid pid, struct State *st) {
	fvec3 pos;

	switch (st->p[pid].team) {
	case 0:
		pos.x = 96.5;
		pos.y = 256.5;
		pos.z = highest_point_spawn(pos.x, pos.y, st);
		break;
	case 1:
		pos.x = 512.5-96;
		pos.y = 256.5;
		pos.z = highest_point_spawn(pos.x, pos.y, st);
		break;
	default:
		pos.x = 256;
		pos.y = 256;
		pos.z = -8;
		break;
	}

	return pos;
}

void on_tool_change(plid pid, unsigned tool, struct State *st) {
	struct PacketSetTool set;

	st->p[pid].tool = tool;

	set.packetID = PacketTypeSetTool;
	set.playerID = pid;
	set.tool = tool;

	SEND(PID_BROADCAST_EXCEPT(pid), set);
}

void send_restock(plid pid, plid from, struct State *st) {
	struct PacketRestock rs;

	rs.packetID = PacketTypeRestock;
	rs.playerID = from;

	SEND(pid, rs);
}

void restock(plid pid, struct State *st) {
	st->p[pid].hp = 100;
	/* TODO: how to handle voxlap and blockaction blocks? do i have to hook on_any/sane_packet for it? */
	st->p[pid].blocks = 50;
	st->p[pid].grenades = 3;

	/* TODO: should i bother broadcasting this? */
	st->f.send_restock(pid, pid, st);
}

void spawn_player(plid pid, struct State *st) {
	struct PacketCreatePlayer cr;
	fvec3 pos;

	st->p[pid].spawntime = 0;
	st->p[pid].team = st->p[pid].newteam;
	/* TODO: do i even need newweapon or just newteam? */
	st->p[pid].weapon = st->p[pid].newweapon;

	/* TODO: what if on_player_spawn doesn't want the player to spawn, and what about spectators */
	st->p[pid].pos = st->f.on_player_spawn(pid, st);
	st->p[pid].ori.x = st->p[pid].team == 0 ? 1 : -1;
	st->p[pid].ori.y = 0;
	st->p[pid].ori.z = 0;
	st->p[pid].vel.x = 0;
	st->p[pid].vel.y = 0;
	st->p[pid].vel.z = 0;
	st->p[pid].inputs = 0;
	st->p[pid].mouseInputs = 0;
	st->p[pid].tool = ToolTypeGun;
	st->p[pid].wade = 0;
	st->p[pid].airborne = 0;
	st->p[pid].alive = st->p[pid].team != 255;
	st->p[pid].blockColor[0] = 111;
	st->p[pid].blockColor[1] = 111;
	st->p[pid].blockColor[2] = 111;
	st->p[pid].hp = 100;
	st->p[pid].blocks = 50;
	st->p[pid].grenades = 3;
	st->p[pid].lastagreedpos.x = st->p[pid].pos.x;
	st->p[pid].lastagreedpos.y = st->p[pid].pos.y;
	st->p[pid].lastagreedpos.z = st->p[pid].pos.z;

	cr.packetID = PacketTypeCreatePlayer;
	cr.playerID = pid;
	cr.weapon = st->p[pid].weapon;
	cr.team = st->p[pid].team;

	/* This +2 is here because *sane* clients always subtract 2 from CreatePlayer z
	 * BetterSpades is not sane, since it was based on piqueserver.
	 */
	cr.pos = st->p[pid].pos;
	cr.pos.z += 2;

	strcpy(cr.name, st->p[pid].name);

	SEND(PID_BROADCAST, cr);
}

const uint8_t ColorFilled[3] = {40, 64, 103};
static int pvx_dump_bitmask_all(uint_fast32_t x, uint_fast32_t y, uint_fast32_t zStart, uint_fast32_t zEnd, const uint8_t *colors, int filled, PVXWriteCallback writecall, void *writeUdata, char *errbuf, void *udata) {
	struct BitmaskUData *data = udata;
	uint_fast32_t z;
	size_t i = x * 64 + y * 512 * 64;
	const uint8_t *color = ColorFilled;

	(void)writecall;
	(void)writeUdata;
	(void)errbuf;

	for (z=zStart;z<=zEnd;z++) {
		pvx_voxel_create4(data->solidData, i, z);
		if (!filled) {
			color = colors;
			colors += 4;
		}

		pvx_voxel_color5(data->colorData, color, i, z);
	}
	return 0;
}

void load_map_from_file(const char *path, struct State *st) {
	struct PVX_VXLStreamConfig config;
	char err[PVX_ERRBUF_SIZE];
	void *buf;
	ssize_t size;

	buf = map_file(path, &size);

	strcpy(err, "no error message provided");

	config.outformat = PVX_FormatCustom;
	config.errbuf = err;
	config.customReadCall = pvx_dump_bitmask_all;
	config.customReadUdata = &st->globals.map;

	memset(st->globals.map.solidData, 0, (512*512*64+7)/8);
	if (pvx_vxl_stream_stateless(buf, size, &config) != 0) {
		/* TODO: recover */
		fprintf(stderr, "pvx_vxl_stream_stateless: %s\n", err);
		exit(EXIT_FAILURE);
	}

	munmap(buf, size);

	/* TODO: don't bother if nobody is home, and deal with compression, reusing the loaded vxl instead of making a new one, whatever */
	/* TODO: what the hell does the previous TODO mean? */
	st->f.send_map(PID_BROADCAST, st);
}

/* You probably want to call this after game end and before map load. TODO: you can figure it out */
/* TODO: rename since it clears score too */
void boot_players_to_limbo(struct State *st) {
	plid i;

	st->globals.teamscore[0] = 0;
	st->globals.teamscore[1] = 0;
	for (i=0;i<MAX_PLAYERS;i++) {
		st->p[i].joined = 0;
		st->p[i].alive = 0;

		st->f.after_player_destroy(i, st);
	}
}

/* Win or timeout or advance or something else. */
/* TODO: wonder if it should be given a reason arg */
/* TODO: do you think lua could add extra args to funcs to pass to other lua scripts? */
void on_game_end(struct State *st) {
	return;
}

void on_position(plid pid, fvec3 pos, struct State *st) {
	st->p[pid].pos = pos;
	st->p[pid].lastagreedpos = pos;
}

void on_orientation(plid pid, fvec3 ori, struct State *st) {
	st->p[pid].ori = ori;
}

void on_move_input(plid pid, unsigned bitmask, struct State *st) {
	struct PacketInput in;

	/* TODO: validate uncrouch? handle openspades jump */
	if ((bitmask & KeyStateTypeCrouch) ^ (st->p[pid].inputs & KeyStateTypeCrouch))
		change_crouch(bitmask & KeyStateTypeCrouch, st->p+pid, st->globals.map.solidData, 0);

	if (bitmask & KeyStateTypeJump && st->p[pid].airborne)
		bitmask &= ~KeyStateTypeJump;

	st->p[pid].inputs = bitmask;

	in.packetID = PacketTypeInput;
	in.playerID = pid;
	in.keyStates = bitmask;

	SEND(PID_BROADCAST_EXCEPT(pid), in);
}

void on_mouse_input(plid pid, unsigned bitmask, struct State *st) {
	struct PacketWeaponInput in;

	st->p[pid].mouseInputs = bitmask;

	in.packetID = PacketTypeWeaponInput;
	in.playerID = pid;
	in.weaponInput = bitmask;

	SEND(PID_BROADCAST_EXCEPT(pid), in);
}

clk on_kill(plid pid, struct State *st) {
	clk now = get_time();

	/* TODO: make less unpredictable/annoying -- players should spawn at the time when being killed would give them the most respawn time, not the most - 1 s */
	/* Each player gets at least 1 s respawn time and at most 8 s */
	return now + from_s(8) - (now % from_s(7));
}

/* TODO: even dead people send position in openspades */
/* TODO: should kill still kill dead men? (probably yes) */
/* TODO: move spawn time into lua */
/* TODO: kill packet only gets sent if pid 0 is joined */
void kill(plid pid, unsigned type, plid killer, struct State *st) {
	struct PacketKill kl;

	st->p[pid].spawntime = st->f.on_kill(pid, st);

	st->p[pid].alive = 0;

	kl.packetID = PacketTypeKill;
	kl.playerID = pid;
	kl.killerID = killer;
	kl.killType = type;
	/* TODO: spawns -- getspawntime func? on_kill? */
	/* TODO: should i call get_time from here or use st->something? */
	kl.respawnTime = to_s(st->p[pid].spawntime - get_time() + 500000000);

	/* Send respawn time to the dead player and spectators, but nobody else. */
	SEND(pid, kl);
	SEND(PID_BROADCAST_TEAM(255), kl);

	kl.respawnTime = 0;
	SEND(PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(255, pid), kl);

	if (type < 4 && pid != killer)
		st->p[killer].score++;

	st->f.after_player_destroy(pid, st);
}

void after_player_destroy(plid pid, struct State *st) {
	return;
}

void set_hp(plid pid, int hp, struct State *st) {
	struct PacketSetHP sh;

	if (hp < 0)
		hp = 0;
	st->p[pid].hp = hp;

	sh.packetID = PacketTypeSetHP;
	sh.hp = hp > 255 ? 255 : hp;
	sh.type = 0;
	memset(&sh.pos, 0, sizeof(sh.pos));

	SEND(pid, sh);
}

void set_hp_directional(plid pid, int hp, fvec3 pos, struct State *st) {
	struct PacketSetHP sh;

	if (hp < 0)
		hp = 0;
	st->p[pid].hp = hp;

	sh.packetID = PacketTypeSetHP;
	sh.hp = hp > 255 ? 255 : hp;
	sh.type = 1;
	sh.pos = pos;

	SEND(pid, sh);
}

/* TODO about grenades: mr. piquespades subtracts player velocity from grenades velocity to get roughly player orientation (len == 1). it then checks if length(that value) is > 2 (why not 1?). if it is, it sets length(that value) to 2 and adds the player velocity back. . . */
/* TODO fun fact: openspades can send SetColor when it's in spectator. . . wonder if dead too? */
/* TODO: what if i don't have a pid to give? */
int get_hit_damage(plid pid, unsigned type, struct State *st) {
	uint8_t dmgmap[3][3] = {
		{49, 100, 33},
		{29, 75, 18},
		{27, 37, 16},
	};

	if (type == HitTypeMelee)
		return 80;

	if (type == HitTypeLegs)
		type = HitTypeArms;

	return dmgmap[st->p[pid].weapon][type];
}

/* TODO: can dead men shoot in openspades if they haven't received a Kill? */
void on_hit(plid pid, unsigned type, plid hitPlayer, struct State *st) {
	if (st->p[pid].team != st->p[hitPlayer].team)
		st->f.set_hp_directional(hitPlayer, st->p[hitPlayer].hp - st->f.get_hit_damage(pid, type, st), st->p[pid].pos, st);

	if (st->p[hitPlayer].hp == 0)
		st->f.kill(hitPlayer, type == HitTypeMelee ? KillTypeMelee : type == HitTypeHead, pid, st);
}

/* TODO: nuke the useless Data from everything, maybe rename WorldUpdate, un-action Kill, annihilate the british, *weapon* reload, . . . */
void on_sane_packet(plid pid, ENetPacket *packet, struct State *st) {
	switch (packet->data[0]) {
#undef PCKT
#define PCKT PositionData
		PCASE
		st->f.on_position(pid, PACKET.pos, st);
		break;
#undef PCKT
#define PCKT OrientationData
		PCASE
		st->f.on_orientation(pid, PACKET.ori, st);
		break;
#undef PCKT
#define PCKT SetColor
		PCASE
		st->f.on_color_change(pid, PACKET.color, st);
		break;
#undef PCKT
#define PCKT Input
		PCASE
		st->f.on_move_input(pid, PACKET.keyStates, st);
		break;
#undef PCKT
#define PCKT WeaponInput
		PCASE
		st->f.on_mouse_input(pid, PACKET.weaponInput & 3, st);
		break;
#undef PCKT
#define PCKT ChatMessage
		PCASE
		st->f.on_chat(pid, PACKET.message, PACKET.type, st);

		break;
#undef PCKT
#define PCKT ExistingPlayer
		PCASE
		if (st->p[pid].joined)
			st->f.on_switch(pid, PACKET.team, PACKET.weapon, st);
		else
			/* TODO: CP437/WIN-1252 */
			st->f.on_join(pid, PACKET.team, PACKET.team == 255 ? 0 : PACKET.weapon, PACKET.name, st);

		break;
#undef PCKT
#define PCKT ShortPlayerData
		PCASE
		st->f.on_switch(pid, PACKET.team, PACKET.weapon, st);
		break;
#undef PCKT
#define PCKT ChangeTeam
		PCASE
		st->f.on_switch(pid, PACKET.team, st->p[pid].newweapon, st);
		break;
#undef PCKT
#define PCKT ChangeWeapon
		PCASE
		st->f.on_switch(pid, st->p[pid].newteam, PACKET.weapon, st);
		break;
#undef PCKT
#define PCKT Hit
		PCASE
		st->f.on_hit(pid, PACKET.type, PACKET.playerID, st);
		break;
#undef PCKT
#define PCKT Grenade
		PCASE
		/* TODO: blocks and grenades validation */
		st->f.on_grenade(pid, PACKET.pos, PACKET.vel, PACKET.fuseLength, st);
		break;
#undef PCKT
#define PCKT BlockAction
		PCASE
		st->f.on_block_action(pid, PACKET.pos, PACKET.type, st);
		break;
#undef PCKT
#define PCKT BlockLine
		PCASE
		st->f.on_block_line(pid, PACKET.start, PACKET.end, st);
		break;
#undef PCKT
#define PCKT SetTool
		PCASE
		st->f.on_tool_change(pid, PACKET.tool, st);
		break;
#undef PCKT
#define PCKT WeaponReload
		PCASE
		st->f.on_reload(pid, PACKET.magazineAmmo, PACKET.reserveAmmo, st);
		break;
	}
}

void send_block_action(plid pid, ivec3 pos, unsigned type, plid from, struct State *st) {
	struct PacketBlockAction ba;

	ba.packetID = PacketTypeBlockAction;
	ba.playerID = from;
	ba.type = type;
	ba.pos = pos;

	SEND(pid, ba);
}

void send_block_line(plid pid, ivec3 start, ivec3 end, plid from, struct State *st) {
	struct PacketBlockLine bl;

	bl.packetID = PacketTypeBlockLine;
	bl.playerID = from;
	bl.start = start;
	bl.end = end;

	SEND(pid, bl);
}

void send_set_color(plid pid, color color, plid from, struct State *st) {
	struct PacketSetColor sc;

	sc.packetID = PacketTypeSetColor;
	sc.playerID = from;
	sc.color[0] = color[0];
	sc.color[1] = color[1];
	sc.color[2] = color[2];

	SEND(pid, sc);
}

void set_color(plid pid, color color, struct State *st) {
	st->p[pid].blockColor[0] = color[0];
	st->p[pid].blockColor[1] = color[1];
	st->p[pid].blockColor[2] = color[2];

	st->f.send_set_color(PID_BROADCAST, color, pid, st);
}

/* TODO: sync player's own block color by making abuse of pid 32? that would be very cursed though */
void on_color_change(plid pid, color color, struct State *st) {
	st->p[pid].blockColor[0] = color[0];
	st->p[pid].blockColor[1] = color[1];
	st->p[pid].blockColor[2] = color[2];

	st->f.send_set_color(PID_BROADCAST_EXCEPT(pid), color, pid, st);
}

void on_block_action(plid pid, ivec3 pos, unsigned type, struct State *st) {
	st->f.block_action(pos, type, type == 0 ? pid : 0, st);
}

void block_line(ivec3 start, ivec3 end, plid from, struct State *st) {
	dcore_block_line(start.x, start.y, start.z, end.x, end.y, end.z, &st->globals.map, st->p[from].blockColor);
	st->f.send_block_line(PID_BROADCAST, start, end, from, st);
}

void on_block_line(plid pid, ivec3 start, ivec3 end, struct State *st) {
	st->f.block_line(start, end, pid, st);
}

void send_position(plid pid, fvec3 pos, struct State *st) {
	struct PacketPositionData pd;

	pd.packetID = PacketTypePositionData;
	pd.pos = pos;

	SEND(pid, pd);
}

void set_position(plid pid, fvec3 pos, struct State *st) {
	st->p[pid].pos = pos;
	st->p[pid].lastagreedpos = pos;

	st->f.send_position(pid, pos, st);
}

void set_jump(plid pid, struct State *st) {
	struct PacketInput ip;

	st->p[pid].inputs |= KeyStateTypeJump;

	ip.packetID = PacketTypeInput;
	ip.playerID = pid;
	ip.keyStates = st->p[pid].inputs;

	SEND(PID_BROADCAST, ip);
}

void send_intel_capture(plid pid, int winning, plid from, struct State *st) {
	struct PacketIntelCapture ic;

	ic.packetID = PacketTypeIntelCapture;
	ic.playerID = from;
	ic.winning = !!winning;

	SEND(pid, ic);
}

void send_intel_pickup(plid pid, plid from, struct State *st) {
	struct PacketIntelPickup ip;

	ip.packetID = PacketTypeIntelPickup;
	ip.playerID = from;

	SEND(pid, ip);
}

void send_intel_drop(plid pid, fvec3 pos, plid from, struct State *st) {
	struct PacketIntelDrop id;

	id.packetID = PacketTypeIntelDrop;
	id.playerID = from;
	id.pos = pos;

	SEND(pid, id);
}

/* TODO: or intel_capture? */
void capture_intel(plid pid, int winning, struct State *st) {
	/* Hope nobody tries this on a spectator. */
	st->globals.teamscore[st->p[pid].team]++;
	st->p[pid].score += 10;
	st->globals.intelplayers[!st->p[pid].team] = -1;

	/* TODO: map load should clear this? */
	/* TODO: state on map load should have customizable default score */
	if (winning) {
		st->globals.teamscore[0] = 0;
		st->globals.teamscore[1] = 0;
	}

	st->f.send_intel_capture(PID_BROADCAST, winning, pid, st);

	if (winning)
		st->f.on_game_end(st);
}

void pickup_intel(plid pid, struct State *st) {
	st->globals.intelplayers[!st->p[pid].team] = pid;
	st->f.send_intel_pickup(PID_BROADCAST, pid, st);
}

/* TODO: or accept team? */
/* TODO: why doesn't capture set a position, just drop? */
/* TODO: shove a position on capture */
void drop_intel(plid pid, fvec3 pos, struct State *st) {
	st->globals.intelplayers[!st->p[pid].team] = -1;
	st->globals.intelpos[!st->p[pid].team] = pos;
	st->f.send_intel_drop(PID_BROADCAST, pos, pid, st);
}

void send_move_object(plid pid, fvec3 pos, unsigned id, unsigned team, struct State *st) {
	struct PacketMoveObject ob;

	ob.packetID = PacketTypeMoveObject;
	ob.objectID = id;
	ob.team = team;
	ob.pos = pos;

	SEND(pid, ob);
}

void move_intel(unsigned team, fvec3 pos, struct State *st) {
	st->globals.intelplayers[team] = -1;
	st->globals.intelpos[team] = pos;
	/* "object" */
	st->f.send_move_object(PID_BROADCAST, pos, team, 0, st);
}

void move_tent(unsigned team, fvec3 pos, struct State *st) {
	st->globals.tentpos[team] = pos;
	/* "object" */
	st->f.send_move_object(PID_BROADCAST, pos, 2|team, 0, st);
}

void on_crap_packet(plid pid, ENetPacket *packet, struct State *st) {
	LOG("%s:%u (#%u) sent crap packet, ID %i, name %s, len %lu, __LINE__: %i\n\t%s", IP(pid), PORT(pid), pid, packet->dataLength > 0 ? packet->data[0] : -1, st->crappacketname, (unsigned long)packet->dataLength, st->crapline, st->crapcond);

	if (packet->dataLength > 0)
	switch (packet->data[0]) {
	case PacketTypePositionData:
		/* OLD TODO: or should it just be a kick -- set to pos or lastagreedpos? */
		/* NEW TODO: probably not a kick considering this has a chance of being validly triggered (in blocks) */
		/* TODO: what if i set_position a dead guy? what if i'd like to spawn where i die? */
		/* TODO: does sending position screw with client's position timing? */
		/* TODO: would it be worth just limiting the magnitude? */
		/* TODO: do i need a function that's just like set_position except used for position resend context? */
		if (st->p[pid].alive) {
			st->p[pid].lastagreedpos = st->p[pid].pos;
			st->f.send_position(pid, st->p[pid].pos, st);
		}
		break;
	}
}

int intercept(ENetHost *host, ENetEvent *event) {
	ENetBuffer buf;

	(void)event;

	if (host->receivedDataLength == 5 && !memcmp(host->receivedData, "HELLO", 5)) {
		buf.data = "HI";
		buf.dataLength = 2;

		enet_socket_send(host->socket, &host->receivedAddress, &buf, 1);

		/* TODO: prevent dos from slow and plentiful enet connections and from HI */
		LOG("%s:%u says HI", host_ip(&host->receivedAddress), host->receivedAddress.port);
		return 1;
	}

	if (host->receivedDataLength == 8 && !memcmp(host->receivedData, "HELLOLAN", 8)) {
		/* TODO: HELLOLAN */
		buf.data = "{}";
		buf.dataLength = 2;

		enet_socket_send(host->socket, &host->receivedAddress, &buf, 1); return 1;

		LOG("%s:%u says HELLOLAN", host_ip(&host->receivedAddress), host->receivedAddress.port);
		return 1;
	}

	return 0;
}

void set_funcs(struct State *st) {
	st->f.tick = tick;
	st->f.on_any_connect = on_any_connect;
	st->f.on_successful_connect = on_successful_connect;
	st->f.on_disconnect = on_disconnect;
	st->f.on_any_packet = on_any_packet;
	st->f.on_sane_packet = on_sane_packet;
	st->f.on_crap_packet = on_crap_packet;
	st->f.send_map = send_map;
	st->f.send_state = send_state;
	st->f.on_join = on_join;
	st->f.on_switch = on_switch;
	st->f.spawn_player = spawn_player;
	st->f.on_player_spawn = on_player_spawn;
	st->f.on_chat = on_chat;
	st->f.send_chat = send_chat;
	st->f.set_fog = set_fog;
	st->f.load_map_from_file = load_map_from_file;
	st->f.on_tool_change = on_tool_change;
	st->f.on_block_action = on_block_action;
	st->f.block_action = block_action;
	st->f.send_block_action = send_block_action;
	st->f.send_connected_players = send_connected_players;
	st->f.send_player_update = send_player_update;
	st->f.on_position = on_position;
	st->f.on_orientation = on_orientation;
	st->f.on_move_input = on_move_input;
	st->f.on_mouse_input = on_mouse_input;
	st->f.on_color_change = on_color_change;
	st->f.send_position = send_position;
	st->f.set_position = set_position;
	st->f.block_line = block_line;
	st->f.on_block_line = on_block_line;
	st->f.send_block_line = send_block_line;
	st->f.on_hit = on_hit;
	st->f.kill = kill;
	st->f.on_kill = on_kill;
	st->f.get_hit_damage = get_hit_damage;
	st->f.set_hp = set_hp;
	st->f.set_hp_directional = set_hp_directional;
	st->f.on_grenade = on_grenade;
	st->f.detonate_grenade = detonate_grenade;
	st->f.set_color = set_color;
	st->f.send_set_color = send_set_color;
	st->f.on_reload = on_reload;
	st->f.send_reload = send_reload;
	st->f.set_jump = set_jump;
	st->f.tick_player_physics = tick_player_physics;
	st->f.send_intel_capture = send_intel_capture;
	st->f.send_intel_pickup = send_intel_pickup;
	st->f.send_intel_drop = send_intel_drop;
	st->f.capture_intel = capture_intel;
	st->f.pickup_intel = pickup_intel;
	st->f.drop_intel = drop_intel;
	st->f.send_state_ctf = send_state_ctf;
	st->f.send_state_tc = send_state_tc;
	st->f.send_restock = send_restock;
	st->f.restock = restock;
	st->f.move_intel = move_intel;
	st->f.send_move_object = send_move_object;
	st->f.after_player_destroy = after_player_destroy;
	st->f.on_game_end = on_game_end;
	st->f.boot_players_to_limbo = boot_players_to_limbo;
	st->f.send_map_start = send_map_start;
	st->f.send_packet = send_packet;
	st->f.send_packet_unreliable = send_packet_unreliable;
	st->f.move_tent = move_tent;
	st->f.send_grenade = send_grenade;
	st->f.register_grenade = register_grenade;
	st->f.spawn_grenade = spawn_grenade;
	st->f.send_fog = send_fog;
}

void set_defaults(struct State *st) {
	fvec3 hidden = {HUGE_VAL, HUGE_VAL, HUGE_VAL};

	st->globals.fog[0] = 255;
	st->globals.fog[1] = 232;
	st->globals.fog[2] = 128;

	strcpy(st->globals.teamname[0], "Blue");
	strcpy(st->globals.teamname[1], "Green");

	st->globals.teamcolor[0][0] = 196;
	st->globals.teamcolor[1][1] = 196;

	/* TODO: it feels a little weird for these to not be fixed by memset */
	st->globals.intelplayers[0] = -1;
	st->globals.intelplayers[1] = -1;
	st->globals.intelpos[0] = hidden;
	st->globals.intelpos[1] = hidden;
	st->globals.tentpos[0] = hidden;
	st->globals.tentpos[1] = hidden;
}

const char *cfg = "config.lua";
/* config_path is relative to root_path, root_path defaults to . */
#define USAGE "usage: %s [-c config_path] [-d root_path]\n"
void parse_args(int argc, char **argv) {
	int ch;

	while ((ch = getopt(argc, argv, "c:d:")) != -1) {switch (ch){
	case 'c':
		cfg = optarg;
		break;
	case 'd':
		if (chdir(optarg) != 0)
			ERR("chdir");
		break;
	default:
		fprintf(stderr, USAGE, argv[0]);
		exit(EXIT_FAILURE);
	}}
}

void hook_lua(const char *cfg, struct State *st);

int main(int argc, char **argv) {
	ENetAddress addr;
	struct State *st;

	parse_args(argc, argv);
	sandbox();

	st = calloc(1, sizeof(struct State));
	if (st == NULL)
		ERR("calloc");

	st->globals.grenadeSize = 256;
	st->globals.grenadeCount = 0;
	st->globals.grenades = calloc(256, sizeof(struct Grenade));
	if (st->globals.grenades == NULL)
		ERR("calloc");

	addr.host = ENET_HOST_ANY;
	addr.port = 32777;

	st->host = bringup_host(&addr);
	if (st->host == NULL)
		ERR("bringup_host");
	st->host->peers[0].data = st;
	st->host->intercept = intercept;

	st->tickrate = (clk)1000000000 / TICKRATE;
	st->epoch = get_time();
	st->nextTickTime = st->epoch + st->tickrate;

	set_funcs(st);
	set_defaults(st);

	if (pvx_create_bitmask(&st->globals.map, 512, 512, 64) != 0) {
		fputs("can't create bitmask, check your memory\n", stderr);
		exit(EXIT_FAILURE);
	}

	stackData = malloc(32*512*512*sizeof(uint32_t));
	rememberedSolidity = calloc(1, 512*512*sizeof(uint64_t));

	hook_lua(cfg, st);

	st->f.load_map_from_file("maps/map.vxl", st);

	while (1)
		do_loop(st);
}
