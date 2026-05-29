#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <inttypes.h>
#define PRIuSIZET "zu"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <isa-l.h>

#include "cull.h"
#include "demoncore.h"
#include "pvx/src/vxl.h"
#include "sandbox.h"
#include "state.h"

/* TODO: can i no-op a map load for clients that won't take direct statedata? */
/* TODO: what happens if a smelly haxor takes the intel out of bounds (in pyspades)?
 * well there's some nice duplicated code, one in player.py and one in gamemodes.py for dropping intel
 * not sure the gamemodes.py one is ever called
 * tc has drop_flag
 */

/* Tick rate in Hz. Every tick physics and such is calculated. */
#define TICKRATE 60

void before_log(struct State *st) {(void)st;return;}
void after_log(struct State *st) {(void)st;return;}

/* TODO: integrate logging with lua better */
#define SEND(pid, data) st->f.send_packet(pid, &(data), sizeof(data), st)
#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define LOG1(x) do {st->f.before_log(st); fputs(x"\n", stderr); st->f.after_log(st);} while (0)

#define SOFTERR(func) LOG(func": %s", strerror(errno))
/* st might not exist here so we use perror instead of SOFTERR */
#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

/* strcpy for string literals -- shuts up OpenBSD warnings */
#define LITCPY(dest, src) memcpy(dest, src, sizeof(src))

struct ColumnStack stack;
struct ColumnStack *stackData = &stack;
uint64_t *rememberedSolidity;
uint64_t *keepSolid;

const uint8_t ColorFilled[3] = {40, 64, 103};

static const char *cfg = "config.lua";
static const char *root_path = NULL;
/* TODO: allow changing port from cfg? and maybe allow
 * unsharing/pledging different crap in a special cfg file too */
static unsigned long memlimit = 0;
static unsigned long port = 32887;

static volatile sig_atomic_t keepRunning = 1;
static struct State *exit_st;

static int cat_strl(char **out, ...) {
	va_list args, args2;
	size_t i, len, outLen = 0, totalSize = 0;

	va_start(args, out);
	va_copy(args2, args);

	for (i=0;;i++) {
		const char *arg = va_arg(args, const char *);
		if (arg == NULL)
			break;
		totalSize += strlen(arg);
	}
	va_end(args);
	len = i;

	*out = malloc(totalSize+1);
	if (*out == NULL)
		return -1;

	for (i=0;i<len;i++) {
		const char *arg = va_arg(args2, const char *);
		size_t inLen = strlen(arg);
		memcpy(*out+outLen, arg, inLen);
		outLen += inLen;
	}
	(*out)[outLen] = 0;

	va_end(args2);
	return 0;
}

static void enet_nomem(void) {
	perror("libENet");
	return;
}

static ENetHost *bringup_host(ENetAddress *addr) {
	ENetHost *host;
	ENetCallbacks callbacks = {0};
	callbacks.no_memory = enet_nomem;

	enet_initialize_with_callbacks(ENET_VERSION, &callbacks);
	/* +1 to allow the 33st player connecting (assuming max is 32) to be sent a disconnect with "server full" as reason */
	host = enet_host_create(addr, MAX_PLAYERS+1, 1, 0, 0);

	if (host == NULL)
		return NULL;

	if (enet_host_compress_with_range_coder(host) != 0) {
		enet_host_destroy(host);
		return NULL;
	}

	host->compressor.compress = NULL;

	return host;
}

/* TODO: should i specify an epoch? */
extern clk get_time(void) {
	struct timespec timeSpec;

	clock_gettime(CLOCK_MONOTONIC, &timeSpec);

	return timeSpec.tv_sec * 1000000000 + timeSpec.tv_nsec;
}

static int time_until(clk ts) {
	clk now = get_time();

	if (ts > now)
		return ts-now;

	return 0;
}

static clk to_ms(clk ts) {
	return ts / 1000000;
}

extern clk to_s(clk ts) {
	return ts / 1000000000;
}

extern double to_s_double(clk ts) {
	return (double)ts / 1000000000;
}

static clk from_s(clk ts) {
	return ts * 1000000000;
}

extern clk from_s_double(double ts) {
	return ts * 1000000000;
}

static void handle_event(ENetEvent *event, struct State *st) {
	switch (event->type) {
	case ENET_EVENT_TYPE_CONNECT:
		st->f.on_any_connect(event->peer->incomingPeerID, st);
		break;
	case ENET_EVENT_TYPE_DISCONNECT:
		st->f.on_disconnect(event->peer->incomingPeerID, st);
		break;
	case ENET_EVENT_TYPE_RECEIVE:
		if (st->f.on_any_packet(event->peer->incomingPeerID, event->packet->data, event->packet->dataLength, st))
			st->f.on_crap_packet(event->peer->incomingPeerID, event->packet->data, event->packet->dataLength, st);
		else
			st->f.on_sane_packet(event->peer->incomingPeerID, event->packet->data, event->packet->dataLength, st);

		enet_packet_destroy(event->packet);
		break;
	case ENET_EVENT_TYPE_NONE:
		break;
	}
}

extern jmp_buf lua_panicenv;
void hook_lua(const char *cfg, unsigned long port, struct State *st);
void setpanic_lua(void);
void close_lua(void);

#define PANICJMP_LUA() setjmp(lua_panicenv);

static void do_loop(struct State *st) {
	ENetEvent event;

	PANICJMP_LUA();

	/* TODO: service main host and masterlist host simultaniously? */
	while (enet_host_service(st->host, &event, to_ms(time_until(st->nextTickTime))) > 0) {
		handle_event(&event, st);
	}

	masterlist_service(&st->ms);

	st->nextTickTime += st->tickrate;
	st->f.tick(st);
}

/* Use this for iterating over BROADCAST_* pids */
extern int pid_matches(plid broadcast, plid pid, struct State *st) {
	uint32_t flags = (uint32_t)broadcast >> 29;
	uint32_t data = (uint32_t)broadcast & (uint32_t)0x1fffffff;

	switch (flags) {
	case 1:
		/* PID_BROADCAST */
		return st->p[pid].connected;
	case 2:
		/* PID_BROADCAST_EXCEPT */
		return (st->p[pid].connected && pid != (plid)data);
	case 3:
		/* PID_BROADCAST_TEAM */
		return (st->p[pid].joined && st->p[pid].team == data);
	case 4:
		/* PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER */
		return (pid != data >> 8 && st->p[pid].connected && (!st->p[pid].joined || (st->p[pid].joined && st->p[pid].team != (data & 0xff))));
	default:
		return broadcast == pid;
	}
}

static void set_fog(color color, struct State *st) {
	st->globals.fog[0] = color[0];
	st->globals.fog[1] = color[1];
	st->globals.fog[2] = color[2];

	st->f.send_fog(PID_BROADCAST, color, st);
}

static int alloc_more_nades(struct State *st) {
	struct Grenade *newbuf;

	newbuf = calloc(st->globals.grenadeSize<<2, sizeof(struct Grenade));
	if (newbuf == NULL)
		return -1;

	memcpy(newbuf, st->globals.grenades, st->globals.grenadeSize*sizeof(struct Grenade));

	st->globals.grenadeSize <<= 2;
	st->globals.grenades = newbuf;
	return 0;
}

/* TODO: it might be a little major bit more efficient to fill in nades starting at the start, not the end */
static int alloc_less_nades(struct State *st) {
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

static void remove_grenade(size_t index, struct State *st) {
	st->globals.grenades[index].exists = 0;

	while (st->globals.grenadeCount > 0 && !st->globals.grenades[st->globals.grenadeCount - 1].exists)
		st->globals.grenadeCount--;

	if (st->globals.grenadeCount < st->globals.grenadeSize && alloc_less_nades(st) == -1)
		LOG("realloc doesn't want to shrink the grenades buffer (%"PRIuSIZET" currently allocated)", st->globals.grenadeSize);
}

static float sqr_len3(fvec3 vec) {
	return vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
}

static float sqr_dist3(fvec3 pos1, fvec3 pos2) {
	pos1.x -= pos2.x;
	pos1.y -= pos2.y;
	pos1.z -= pos2.z;

	return sqr_len3(pos1);
}

static float dist1(float p1, float p2) {
	return fabsf(p1 - p2);
}

static float safe_sqr_dist3(fvec3 pos1, fvec3 pos2) {
	float val = sqr_dist3(pos1, pos2);

	if (val == 0)
		return 1;

	return val;
}

static int libspades_voxel_bounds_check_player(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z) {
	return (x < MAP_SIZE_X && y < MAP_SIZE_Y && z < (MAP_SIZE_Z - 2));
}

static void set_solid(ivec3 pos, struct State *st) {
	pvx_voxel_create4(st->globals.map.solidData, CALC_I(pos.x, pos.y), pos.z);
}

static void set_vox_color(ivec3 pos, color clr, struct State *st) {
	pvx_voxel_color5(st->globals.map.colorData, clr, CALC_I(pos.x, pos.y), pos.z);
}

static void set_empty3(int32_t x, int32_t y, int32_t z, struct State *st) {
	pvx_voxel_destroy4(st->globals.map.solidData, CALC_I(x, y), z);
}

static void set_empty(ivec3 pos, struct State *st) {
	set_empty3(pos.x, pos.y, pos.z, st);
}

static int get_solid3(int32_t x, int32_t y, int32_t z, struct State *st) {
	return pvx_voxel_get_solidity4(st->globals.map.solidData, CALC_I(x, y), z);
}

extern int get_solid(ivec3 pos, struct State *st) {
	return get_solid3(pos.x, pos.y, pos.z, st);
}

static void cull3(int32_t x, int32_t y, int32_t z, struct State *st) {
	cull_floating_voxels(x, y, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
}

/* TODO: culling on openspades is much faster if there's a "floor" to connect to near the bottom
 * |#|
 * |#|
 * | |
 * |-|
 *   |
 * Say you're trying to destroy those two # blocks; it'll be much faster with that - than without it
 * This is when facing {-1,0,0}
 */
static void cull_grenade(uint_fast32_t x,
                         uint_fast32_t y,
                         uint_fast32_t z,
                         int_fast8_t xOffset,
                         int_fast8_t yOffset,
                         int_fast8_t zOffset,
                         struct State *st) {
	if (xOffset != 0 && yOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y + yOffset, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		cull_floating_voxels(x + xOffset, y + yOffset * 2, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		cull_floating_voxels(x + xOffset, y + yOffset, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (xOffset != 0 && yOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y + yOffset, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		cull_floating_voxels(x + xOffset, y + yOffset * 2, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (xOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		cull_floating_voxels(x + xOffset, y, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (yOffset != 0 && zOffset != 0) {
		cull_floating_voxels(x, y + yOffset * 2, z + zOffset, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		cull_floating_voxels(x, y + yOffset, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (xOffset != 0) {
		cull_floating_voxels(x + xOffset * 2, y, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (yOffset != 0) {
		cull_floating_voxels(x, y + yOffset * 2, z, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
	if (zOffset != 0) {
		cull_floating_voxels(x, y, z + zOffset * 2, 1, st->globals.map.solidData, stackData, (void *)rememberedSolidity, (void *)keepSolid);
		return;
	}
}

static void destroyGrenadeVoxel(uint_fast32_t x,
                                uint_fast32_t y,
                                uint_fast32_t z,
				uint_fast32_t *solids,
				uint_fast32_t ctr,
                                int_fast8_t xOffset,
                                int_fast8_t yOffset,
                                int_fast8_t zOffset,
                                struct State *st) {
	if (!libspades_voxel_bounds_check_player(x + xOffset, y + yOffset, z + zOffset) ||
	    !pvx_voxel_get_solidity4(st->globals.map.solidData, CALC_I(x + xOffset, y + yOffset), z + zOffset))
		return;

	pvx_voxel_destroy4(st->globals.map.solidData, CALC_I(x + xOffset, y + yOffset), z + zOffset);
	*solids |= ctr;
}

static void send_cull(int32_t x, int32_t y, int32_t z, int noneighbor, struct State *st) {
	plid i;
	ivec3 pos;

	if (!(x >= 0 && x < 512 && y >= 0 && y < 512 && z >= 0 && z < 62))
		return;

	pos.x = x;
	pos.y = y;
	pos.z = z;

	if (st->globals.cullPersonality == CULL_PERSONALITY_OPENSPADES) {
		if (noneighbor)
		for (i=0;i<MAX_PLAYERS;i++) {
			if (pid_matches(PID_BROADCAST, i, st) && !(st->p[i].bugMask & QUIRK_OS_BACTION_CULL))
				st->f.send_block_action(i, pos, 1, 32, st);
		}
	} else {
		if (!noneighbor) {
			const int xoffs[6] = {-1,  1,  0,  0,  0,  0};
			const int yoffs[6] = { 0,  0, -1,  1,  0,  0};
			const int zoffs[6] = { 0,  0,  0,  0, -1,  1};
			unsigned j;

			for (j=0;j<6;j++) {
				pos.x = x + xoffs[j];
				pos.y = y + yoffs[j];
				pos.z = z + zoffs[j];

				if (pos.x >= 0 && pos.x < 512 && pos.y >= 0 && pos.y < 512 && pos.z >= 0 && pos.z < 62 && !get_solid3(pos.x, pos.y, pos.z, st))
				for (i=0;i<MAX_PLAYERS;i++) {
					if (pid_matches(PID_BROADCAST, i, st) && st->p[i].bugMask & QUIRK_OS_BACTION_CULL)
						st->f.send_block_action(i, pos, 1, 32, st);
				}
			}
		} else for (i=0;i<MAX_PLAYERS;i++) {
			if (pid_matches(PID_BROADCAST, i, st) && st->p[i].bugMask & QUIRK_OS_BACTION_CULL)
				st->f.send_block_action(i, pos, 1, 32, st);
		}
	}
}

static void grenade_cullblocks(ivec3 pos, uint32_t solids, struct State *st) {
	ivec3 off;
	uint_fast32_t ctr = 1;

	for (off.z = -1; off.z <= 1; off.z++) {
		for (off.y = -1; off.y <= 1; off.y++) {
			for (off.x = -1; off.x <= 1; off.x++) {
				if (solids & ctr || st->globals.cullPersonality == CULL_PERSONALITY_VOXLAP) {
					cull_grenade(pos.x, pos.y, pos.z, off.x, off.y, off.z, st);
				} if (!(solids & 0x20000000))
					send_cull(pos.x + off.x, pos.y + off.y, pos.z + off.z, solids & ctr, st);

				ctr <<= 1;
			}
		}
	}
}

static uint32_t grenade_rmblocks(ivec3 pos, struct State *st) {
	ivec3 off;
	uint_fast32_t solids = 0;
	uint_fast32_t ctr = 1;

	/* TODO: move bounds checking to each of these for loops? or is that terrible */
	for (off.z = -1; off.z <= 1; off.z++) {
		for (off.y = -1; off.y <= 1; off.y++) {
			for (off.x = -1; off.x <= 1; off.x++) {
				destroyGrenadeVoxel(pos.x, pos.y, pos.z, &solids, ctr, off.x, off.y, off.z, st);
				ctr <<= 1;
			}
		}
	}

	return solids;
}

static void unpristine(struct State *st) {
	if (st->globals.pristineBuf) {
		munmap(st->globals.pristineBuf, st->globals.pristineLen);
		st->globals.pristineBuf = NULL;
	}
}

/* Don't try to build with this! */
static uint32_t block_action_rm(ivec3 pos, unsigned type, plid from, struct State *st) {
	uint32_t mask = 0;
	int full = 0;

	unpristine(st);

	switch (type) {
	case 1: /* destroy */
		/* Don't cull (or destroy) if nonsolid, which'll only ever happen for fun scripts */
		if (get_solid(pos, st)) {
			set_empty(pos, st);
			mask = 1;
			full = 1;
		}
		break;
	case 2: /* 3x destroy */
		/* TODO: only do cull on actually destroyed voxels in rl */
		/* TODO: do i need to verify that pos.z < 62 here, or do i trust that all calls have valid position? what do the clients do? */
		if (pos.z < 62 && get_solid3(pos.x, pos.y, pos.z, st)) {
			set_empty(pos, st);
			mask |= 1;
		} if (pos.z < 61 && get_solid3(pos.x, pos.y, pos.z+1, st)) {
			mask |= 2;
			set_empty3(pos.x, pos.y, pos.z+1, st);
		} if (pos.z > 0 && get_solid3(pos.x, pos.y, pos.z-1, st)) {
			set_empty3(pos.x, pos.y, pos.z-1, st);
			mask |= 4;
		}

		full = mask == 7;
		break;
	case 3: /* nade destroy */
		mask = grenade_rmblocks(pos, st);
		full = (mask & 0x7ffdfff) == 0x7ffdfff;
		break;
	}

	if (mask == 0 && st->globals.cullPersonality == CULL_PERSONALITY_OPENSPADES)
		return 0;

	if (!st->globals.loadingMap) {
		plid i;

		if (full || st->globals.cullPersonality == CULL_PERSONALITY_VOXLAP)
			st->f.send_block_action(PID_BROADCAST, pos, type, from, st);
		else for (i=0;i<MAX_PLAYERS;i++) {
			/* This also handles OpenSpades decreasing its block count on packet recv */
			if (pid_matches(PID_BROADCAST, i, st) && st->p[i].bugMask & QUIRK_OS_BACTION_CULL)
				st->f.send_block_action(i, pos, type, from, st);
		}
	}

	/* full here is 0x20000000 if true */
	return (type << 30) | (full << 29) | mask;
}

static void block_action_cull(ivec3 pos, uint32_t mask, struct State *st) {
	uint32_t type = mask >> 30;

	unpristine(st);

	switch (type) {
	case 1: /* Destroy */
		cull3(pos.x-1, pos.y, pos.z, st);
		cull3(pos.x+1, pos.y, pos.z, st);
		cull3(pos.x, pos.y-1, pos.z, st);
		cull3(pos.x, pos.y+1, pos.z, st);
		cull3(pos.x, pos.y, pos.z-1, st);
		cull3(pos.x, pos.y, pos.z+1, st);
		break;
	case 2: /* 3x destroy */
		if (mask & 4 || st->globals.cullPersonality == CULL_PERSONALITY_VOXLAP) {
			cull3(pos.x-1, pos.y, pos.z-1, st);
			cull3(pos.x+1, pos.y, pos.z-1, st);
			cull3(pos.x, pos.y-1, pos.z-1, st);
			cull3(pos.x, pos.y+1, pos.z-1, st);
			cull3(pos.x, pos.y, pos.z-2, st);
		} if (!(mask & 0x20000000))
			send_cull(pos.x, pos.y, pos.z - 1, mask & 4, st);

		if (mask & 2 || st->globals.cullPersonality == CULL_PERSONALITY_VOXLAP) {
			cull3(pos.x-1, pos.y, pos.z+1, st);
			cull3(pos.x+1, pos.y, pos.z+1, st);
			cull3(pos.x, pos.y-1, pos.z+1, st);
			cull3(pos.x, pos.y+1, pos.z+1, st);
			cull3(pos.x, pos.y, pos.z+2, st);
		} if (!(mask & 0x20000000))
			send_cull(pos.x, pos.y, pos.z + 1, mask & 2, st);

		if (mask & 1 || st->globals.cullPersonality == CULL_PERSONALITY_VOXLAP) {
			cull3(pos.x-1, pos.y, pos.z, st);
			cull3(pos.x+1, pos.y, pos.z, st);
			cull3(pos.x, pos.y-1, pos.z, st);
			cull3(pos.x, pos.y+1, pos.z, st);
		} if (!(mask & 0x20000000))
			send_cull(pos.x, pos.y, pos.z, mask & 1, st);

		break;
	case 3: /* Nade destroy */
		grenade_cullblocks(pos, mask, st);
		break;
	default:
		return;
	}
}

/* st->f.finish_cull under a different name because the name finish_cull was already taken */
static void fin_cull(struct State *st) {
	/* TODO: move stackData and whatever into st unless you really want to share it across states */
	(void)st;
	finish_cull(stackData, (void *)keepSolid);
}

static void block_action(ivec3 pos, unsigned type, plid from, struct State *st) {
	switch (type) {
	case 0: /* Build */
		unpristine(st);
		set_solid(pos, st);
		set_vox_color(pos, st->p[from].blockColor, st);
		if (!st->globals.loadingMap)
			st->f.send_block_action(PID_BROADCAST, pos, type, from, st);
		break;
	default: /* Any of the destroy family */
		st->f.block_action_cull(pos, st->f.block_action_rm(pos, type, from, st), st);
		st->f.finish_cull(st);
		break;
	}
}

/* Fun fact: since distance is limited to 16 along each axis, the minimum grenade damage is 5. */
static void detonate_grenade(size_t index, struct State *st) {
	plid i;
	ivec3 ipos;
	struct Grenade nade = st->globals.grenades[index];

	/* TODO: move this to the end of the func? */
	st->f.remove_grenade(index, st);

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
			st->f.damage_player_directional(
				i,
				4096 / safe_sqr_dist3(nade.pos, st->p[i].pos),
				nade.pos,
				KillTypeGrenade,
				nade.pid,
				st
			);
		}
	}

	ipos.x = floorf(nade.pos.x);
	ipos.y = floorf(nade.pos.y);
	ipos.z = floorf(nade.pos.z);

	st->f.block_action(ipos, BlockActionTypeGrenadeDestroy, nade.pid, st);
}

static size_t register_grenade(plid pid, unsigned team, fvec3 pos, fvec3 vel, clk fuse, struct State *st) {
	if (st->globals.grenadeCount == st->globals.grenadeSize && alloc_more_nades(st) == -1) {
		LOG("Out of memory for more grenades (%"PRIuSIZET" currently allocated)", st->globals.grenadeSize);
		return (size_t)-1;
	}

	st->globals.grenades[st->globals.grenadeCount].detonateTime = get_time()+fuse;
	st->globals.grenades[st->globals.grenadeCount].pos = pos;
	st->globals.grenades[st->globals.grenadeCount].vel = vel;
	st->globals.grenades[st->globals.grenadeCount].pid = pid;
	st->globals.grenades[st->globals.grenadeCount].team = team;
	st->globals.grenades[st->globals.grenadeCount++].exists = 1;

	return st->globals.grenadeCount-1;
}

static size_t spawn_grenade(plid pid, unsigned team, fvec3 pos, fvec3 vel, clk fuse, struct State *st) {
	size_t idx = st->f.register_grenade(pid, team, pos, vel, fuse, st);
	if (idx != (size_t)-1)
		st->f.send_grenade(PID_BROADCAST, pos, vel, to_s_double(fuse), 0, st);
	return idx;
}

#if 1
static void tick_player_physics(plid pid, float timeDelta, struct State *st) {
	move_player(st->p+pid, timeDelta, st->globals.map.solidData, 0); /* TODO: LOOP_PHYSICS (as a script?) */
}
#else
/* Experiment with physics looping */
static void tick_player_physics(plid pid, float timeDelta, struct State *st) {
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

/* These two are hardcoded into the protocol and set with the CreatePlayer (both) and Restock (just reserve) packets */
/* TODO: it may be worth abstracting around this some more */
const uint8_t initialMagAmmo[3] = {
	10,
	30,
	6
};

const uint8_t initialReserveAmmo[3] = {
	50,
	120,
	48
};


/* TODO: can this be altered? */
/* TODO: should canceled reload send a reload packet with current estimation in it? */
const clk reloadTime[3] = {
	/* The first two are 2.5 s, the last is 0.5 s */
	2500000000,
	2500000000,
	 500000000 /* This last one is annoying and repeats the reload a lot */
};

const clk fireTime[3] = {
	 500000000,
	 100000000,
	1000000000
};

static void tick(struct State *st) {
	size_t i;
	clk now = get_time();

	for (i=0;i<MAX_PLAYERS;i++) {
		if (!st->p[i].joined)
			continue;

		if (st->p[i].alive)
			st->f.tick_player_physics(i, (double)st->tickrate/1000000000, st);

		if (st->p[i].spawntime && now >= st->p[i].spawntime)
			st->f.spawn_player(i, st->f.get_spawn_position(i, st), st);

		if (st->p[i].reloadtime && now >= st->p[i].reloadtime)
			st->f.reload_player(i, st);

		if (st->p[i].estfiretime && now >= st->p[i].estfiretime) {
			if (st->p[i].tool == ToolTypeGun && st->p[i].mouseInputs & 1) {
				st->p[i].estfiretime += fireTime[st->p[i].gun];

				st->f.before_estimated_fire(i, st);
				if (st->p[i].estMagAmmo != 0)
					st->p[i].estMagAmmo--;
			} else
				st->p[i].estfiretime = 0;
		}
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

	st->f.send_player_update(PID_BROADCAST, st);
}

static ssize_t get_fd_size(int fildes, struct State *st) {
	struct stat sb;

	if (fstat(fildes, &sb) != 0) {
		SOFTERR("fstat");
		return -1;
	}

	return sb.st_size;
}

/* MAP_FAILED is a hack that gives us a 2nd return value for nonexistant files */
#define MAP_FILE_ENOENT MAP_FAILED
/* TODO: static literally all the functions */
static void *map_file(const char *path, ssize_t *size, struct State *st) {
	int fd;
	void *mem;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		if (errno == ENOENT)
			return MAP_FILE_ENOENT;
		SOFTERR("open");
		return NULL;
	}

	*size = get_fd_size(fd, st);
	if (*size == -1) {
		while (close(fd) == -1 && errno == EINTR);
		return NULL;
	}

	mem = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (mem == MAP_FAILED) {
		SOFTERR("mmap");
		while (close(fd) == -1 && errno == EINTR);
		return NULL;
	}

	while (close(fd) == -1 && errno == EINTR);
	return mem;
}

static int32_t highest_point(int32_t x, int32_t y, struct State *st) {
	int32_t z;

	for (z=0;z<64;z++) {
		if (get_solid3(x, y, z, st))
			return z;
	}

	return 64;
}

static float highest_point_spawn(int32_t x, int32_t y, struct State *st) {
	float z;

	z = highest_point(x, y, st);
	if (z == 63)
		z = 64;

	z -= 2.251;
	
	return z;
}

static fvec3 get_spawn_position(plid pid, struct State *st) {
	fvec3 pos;

	switch (st->p[pid].newteam) {
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

static void restock(plid pid, struct State *st) {
	st->p[pid].hp = 100;
	/* TODO: how to handle voxlap and blockaction blocks? do i have to hook on_any/sane_packet for it? */
	st->p[pid].blocks = 50;
	st->p[pid].grenades = 3;
	st->p[pid].reserveAmmo = initialReserveAmmo[st->p[pid].gun];

	/* TODO: should i bother broadcasting this? */
	st->f.send_restock(pid, pid, st);
}

/* TODO: make this take a position arg and default it to get_spawn_position() */
static void spawn_player(plid pid, fvec3 pos, struct State *st) {
	/* The player *should* already be connected by now, and if it isn't
	 * then consistency will definitely get screwed, but at least it won't
	 * be """that""" screwed if I set connected to 1 here. . . right?
	 *
	 * Maybe I should call just on_successful_connect() here. . .
	 */
	st->p[pid].connected = 1;

	st->p[pid].joined = 1;
	st->p[pid].alive = st->p[pid].newteam != 255;

	st->p[pid].spawntime = 0;
	st->p[pid].reloadtime = 0;
	st->p[pid].estfiretime = 0;
	st->p[pid].team = st->p[pid].newteam;
	/* TODO: do i even need newgun or just newteam? */
	st->p[pid].gun = st->p[pid].newgun;

	st->p[pid].pos = pos;
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
	st->p[pid].blockColor[0] = 111;
	st->p[pid].blockColor[1] = 111;
	st->p[pid].blockColor[2] = 111;
	st->p[pid].hp = 100;
	st->p[pid].blocks = 50;
	st->p[pid].grenades = 3;
	st->p[pid].estMagAmmo = initialMagAmmo[st->p[pid].gun];
	/* If using shotgun (2), multiply maxMagAmmo by 8 to deal with its multiple pellets until i bother validating bullet timing too */
	st->p[pid].maxMagAmmo = initialMagAmmo[st->p[pid].gun] * (1 + (st->p[pid].gun == 2)*7);
	st->p[pid].reserveAmmo = initialReserveAmmo[st->p[pid].gun];
	st->p[pid].lastagreedpos.x = st->p[pid].pos.x;
	st->p[pid].lastagreedpos.y = st->p[pid].pos.y;
	st->p[pid].lastagreedpos.z = st->p[pid].pos.z;

	pos.z += 2;
	st->f.send_spawn_player(PID_BROADCAST, pos, st->p[pid].gun, st->p[pid].team, st->p[pid].name, pid, st);
}

static void set_ammo(plid pid, unsigned mag, unsigned reserve, struct State *st) {
	st->p[pid].estMagAmmo = mag;
	st->p[pid].maxMagAmmo = mag * (1 + (st->p[pid].gun == 2)*7);
	st->p[pid].reserveAmmo = reserve;

	st->f.send_reload(pid, mag, reserve, pid, st);
}

static void reload_player(plid pid, struct State *st) {
	unsigned ammo = st->p[pid].estMagAmmo < st->p[pid].maxMagAmmo ? st->p[pid].estMagAmmo : st->p[pid].maxMagAmmo;
	unsigned transfer = initialMagAmmo[st->p[pid].gun] - ammo;

	/* TODO: make sure you handle 0 mag/reserve ammo properly everywhere */
	if (transfer > st->p[pid].reserveAmmo)
		transfer = st->p[pid].reserveAmmo;

	if (ammo >= initialMagAmmo[st->p[pid].gun] || transfer == 0) {
		st->p[pid].reloadtime = 0;
		return;
	}

	/* TODO: no reloading *and* firing, except maybe with the shotgun */
	if (st->p[pid].gun == 2) {
		/* TODO: either that last TODO or stop reloading when mag is max */
		transfer = 1;
		st->p[pid].reloadtime += reloadTime[st->p[pid].gun];
	} else
		st->p[pid].reloadtime = 0;

	st->f.set_ammo(pid, ammo+transfer, st->p[pid].reserveAmmo-transfer, st);
}

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

/* TODO: should it send packets? */
static void clear_map(struct State *st) {
	memset(st->globals.map.solidData, 0, (512*512*64+7)/8);
	unpristine(st);
}

static void prepare_map_load(struct State *st) {
	st->globals.loadingMap = 1;
	clear_map(st);
}

static void finish_map_load(struct State *st) {
	plid i;

	st->globals.loadingMap = 0;

	/* TODO: don't bother reencoding the vxl if we can reuse the loaded one */
	/* Only send map if someone's actually connected */
	for (i=0;i<MAX_PLAYERS;i++) {
		if (pid_matches(PID_BROADCAST, i, st)) {
			st->f.send_map(PID_BROADCAST, st);
			break;
		}
	}
}

static int load_vxl_from_mem(const void *data, size_t len, struct State *st) {
	struct PVX_VXLStreamConfig config;
	char err[PVX_ERRBUF_SIZE];

	LITCPY(err, "no error message provided");

	config.outformat = PVX_FormatCustom;
	config.errbuf = err;
	config.customReadCall = pvx_dump_bitmask_all;
	config.customReadUdata = &st->globals.map;

	if (pvx_vxl_stream_stateless(data, len, &config) != 0) {
		/* TODO: recover */
		LOG("pvx_vxl_stream_stateless: %s", err);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/* Isn't this one similar to begin_load_vxl_from_file. . . Only difference is no prepare. (could be useful for mapscripts writing map then loading vxl after?) Should i kill it? */
static int load_vxl_from_file(const char *path, struct State *st) {
	void *buf;
	ssize_t size;

	buf = map_file(path, &size, st);
	if (buf == NULL)
		return -1;
	if (buf == MAP_FILE_ENOENT)
		return -2;

	st->f.load_vxl_from_mem(buf, size, st);

	munmap(buf, size);

	return 0;
}

static int begin_load_vxl_from_file(const char *path, struct State *st) {
	void *buf;
	ssize_t size;

	buf = map_file(path, &size, st);
	if (buf == NULL)
		return -1;
	if (buf == MAP_FILE_ENOENT)
		return -2;

	st->f.prepare_map_load(st);
	st->f.load_vxl_from_mem(buf, size, st);

	munmap(buf, size);

	return 0;
}

/* TODO: decompress zlib instead of using its associated .vxl friend */
static int load_map(const char *name, struct State *st) {
	void *buf;
	char *strbuf;
	ssize_t size;

	if (st->f.begin_load_vxl_from_file(name, st) < 0) {
		int err;

		if (cat_strl(&strbuf, "maps/", name, ".vxl", NULL) == -1) {
			SOFTERR("malloc");
			return -1;
		}

		err = st->f.begin_load_vxl_from_file(strbuf, st);
		free(strbuf);

		if (err < 0) {
			if (err == -2)
				SOFTERR("open");
			return -1;
		}
	}

	do {
		if (cat_strl(&strbuf, name, ".zlib", NULL) == -1) {
			SOFTERR("malloc");
			buf = NULL;
			break;
		}

		buf = map_file(strbuf, &size, st);
		free(strbuf);

		if (buf == NULL || buf == MAP_FILE_ENOENT) {
			if (cat_strl(&strbuf, "maps/", name, ".vxl.zlib", NULL) == -1) {
				SOFTERR("malloc");
				break;
			}

			buf = map_file(strbuf, &size, st);
			free(strbuf);
		}
	} while (0);

	if (buf != NULL && buf != MAP_FILE_ENOENT) {
		st->globals.pristineBuf = buf;
		st->globals.pristineLen = size;
	}

	st->f.boot_players_to_limbo(st);
	st->f.finish_map_load(st);

	return 0;
}

/* You probably want to call this after game end and before map load. TODO: you can figure it out */
/* TODO: rename since it clears score too */
static void boot_players_to_limbo(struct State *st) {
	plid i;

	st->globals.teamscore[0] = 0;
	st->globals.teamscore[1] = 0;
	for (i=0;i<MAX_PLAYERS;i++) {
		int wasalive = st->p[i].alive;

		/* TODO: holy fragility with these timers */
		st->p[i].joined = 0;
		st->p[i].alive = 0;
		st->p[i].spawntime = 0;
		st->p[i].reloadtime = 0;
		st->p[i].estfiretime = 0;

		if (wasalive)
			st->f.after_player_destroy(i, st);
	}
}

/* TODO: separate out into dedicated send funcs */
/* TODO: account for endianness in dedicated send funcs */
/* TODO: unpack dedicated send funcs??? */
static void demand_fingerprint(plid pid, struct State *st) {
	plid i;

	struct PacketHandshakeInit hi = {PacketTypeHandshakeInit, 0xdeadbeef};
	struct PacketVersionRequest vr = {PacketTypeVersionRequest};

	for (i=0;i<MAX_PLAYERS;i++) {
		if (pid_matches(pid, i, st)) {
			st->p[i].wantFingerprint = 3;

			/* TODO: maybe just send handshake once instead of every call until recieved?
			 * i.e. use initStateSent. . . makes the function less useful though
			 */
			if (!st->p[i].handshaked)
				SEND(i, hi);
		}
	}

	SEND(pid, vr);
	st->f.send_packet(pid, "\x21\x00\x01\x02", 4, st);
}

static clk get_spawn_time(plid pid, struct State *st) {
	clk now = get_time();
	(void)pid;
	(void)st;

	/* TODO: make less unpredictable/annoying -- players should spawn at the time when being killed would give them the most respawn time, not the most - 1 s */
	/* Each player gets at least 1 s respawn time and at most 8 s */
	return now + from_s(8) - (now % from_s(7));
}

/* TODO: even dead people send position in openspades */
/* TODO: should kill still kill dead men? (probably yes) */
/* TODO: move spawn time into lua */
/* TODO: kill packet only gets sent if pid 0 is joined */
/* This one is named func_kill instead of kill because POSIX took that name. */
static void func_kill(plid pid, unsigned type, plid killer, struct State *st) {
	clk now;
	clk delta;
	int wasalive = st->p[pid].alive;

	st->p[pid].spawntime = st->f.get_spawn_time(pid, st);
	st->p[pid].reloadtime = 0;
	st->p[pid].estfiretime = 0;

	st->p[pid].alive = 0;

	now = get_time();
	delta = st->p[pid].spawntime - now;

	if (st->p[pid].spawntime < now)
		delta = 0;

	st->f.send_kill(PID_BROADCAST, delta, type, killer, pid, st);

	/* TODO: type < 4 may have inconsistent handling across clients */
	if (type < 4 && pid != killer)
		st->p[killer].score++;

	/* Just in case someone wants to kill someone extra dead */
	if (wasalive)
		st->f.after_player_destroy(pid, st);
}

static void after_player_destroy(plid pid, struct State *st) {
	(void)pid;
	(void)st;
	return;
}
static void before_estimated_fire(plid pid, struct State *st) {
	(void)pid;
	(void)st;
	return;
}

static void set_hp(plid pid, int hp, struct State *st) {
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

static void set_hp_directional(plid pid, int hp, fvec3 pos, struct State *st) {
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

void damage_player(plid pid, int hp, unsigned type, plid damager, struct State *st) {
	st->f.set_hp(pid, st->p[pid].hp - hp, st);

	if (st->p[pid].hp == 0)
		st->f.kill(pid, type, damager, st);
}

void damage_player_directional(plid pid, int hp, fvec3 pos, unsigned type, plid damager, struct State *st) {
	st->f.set_hp_directional(pid, st->p[pid].hp - hp, pos, st);

	if (st->p[pid].hp == 0)
		st->f.kill(pid, type, damager, st);
}

static void server_msg(plid pid, const char *msg, struct State *st) {
	st->f.send_chat(pid, msg, 2, 0, st);
}

static void player_msg(const char *msg, unsigned type, plid from, struct State *st) {
	st->f.send_chat(type == ChatTypeAll ? PID_BROADCAST : PID_BROADCAST_TEAM(st->p[from].team), msg, type, from, st);
}

/* TODO about grenades: mr. piquespades subtracts player velocity from grenades velocity to get roughly player orientation (len == 1). it then checks if length(that value) is > 2 (why not 1?). if it is, it sets length(that value) to 2 and adds the player velocity back. . . */
/* TODO fun fact: openspades can send SetColor when it's in spectator. . . wonder if dead too? */
/* TODO: what if i don't have a pid to give? */
static int get_hit_damage(plid pid, unsigned type, struct State *st) {
	uint8_t dmgmap[3][3] = {
		{49, 100, 33},
		{29, 75, 18},
		{27, 37, 16},
	};

	if (type == HitTypeMelee)
		return 80;

	if (type == HitTypeLegs)
		type = HitTypeArms;

	return dmgmap[st->p[pid].gun][type];
}

static void set_block_color(plid pid, color color, struct State *st) {
	st->p[pid].blockColor[0] = color[0];
	st->p[pid].blockColor[1] = color[1];
	st->p[pid].blockColor[2] = color[2];

	st->f.send_set_block_color(PID_BROADCAST, color, pid, st);
}

static void block_line(ivec3 start, ivec3 end, plid from, struct State *st) {
	unpristine(st);

	dcore_block_line(start.x, start.y, start.z, end.x, end.y, end.z, &st->globals.map, st->p[from].blockColor);
	if (!st->globals.loadingMap)
		st->f.send_block_line(PID_BROADCAST, start, end, from, st);
}

static void set_position(plid pid, fvec3 pos, struct State *st) {
	st->p[pid].pos = pos;
	st->p[pid].lastagreedpos = pos;

	st->f.send_position(pid, pos, st);
}

static void set_orientation(plid pid, fvec3 ori, struct State *st) {
	st->p[pid].ori = ori;
	st->f.send_orientation(pid, ori, st);
}

static void set_jump(plid pid, struct State *st) {
	st->p[pid].inputs |= KeyStateTypeJump;
	st->f.send_move_input(PID_BROADCAST, st->p[pid].inputs, pid, st);
}

static void set_tool(plid pid, unsigned tool, struct State *st) {
	st->p[pid].tool = tool;
	st->p[pid].reloadtime = 0;

	st->f.send_set_tool(PID_BROADCAST, tool, pid, st);

	/* TODO: set mouseInputs to 0? (and account for buggyshits) */
	if (st->p[pid].estfiretime == 0 && st->p[pid].tool == ToolTypeGun && st->p[pid].mouseInputs & 1) {
		st->p[pid].estfiretime = get_time() + fireTime[st->p[pid].gun];
		/* TODO: does this actually need to be here? */
		st->p[pid].reloadtime = 0;

		st->f.before_estimated_fire(pid, st);
		if (st->p[pid].estMagAmmo != 0)
			st->p[pid].estMagAmmo--;
	}
}


/* TODO: or intel_capture? */
static void capture_intel(plid pid, int winning, struct State *st) {
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

static void pickup_intel(plid pid, struct State *st) {
	st->globals.intelplayers[!st->p[pid].team] = pid;
	st->f.send_intel_pickup(PID_BROADCAST, pid, st);
}

/* TODO: or accept team? */
/* TODO: why doesn't capture set a position, just drop? */
/* TODO: shove a position on capture */
static void drop_intel(plid pid, fvec3 pos, struct State *st) {
	st->globals.intelplayers[!st->p[pid].team] = -1;
	st->globals.intelpos[!st->p[pid].team] = pos;
	st->f.send_intel_drop(PID_BROADCAST, pos, pid, st);
}

static void move_intel(unsigned team, fvec3 pos, struct State *st) {
	st->globals.intelplayers[team] = -1;
	st->globals.intelpos[team] = pos;
	/* "object" */
	st->f.send_move_object(PID_BROADCAST, pos, team, 0, st);
}

static void move_tent(unsigned team, fvec3 pos, struct State *st) {
	st->globals.tentpos[team] = pos;
	/* "object" */
	st->f.send_move_object(PID_BROADCAST, pos, 2|team, 0, st);
}

static void load_initial_map(struct State *st) {
	st->f.load_map("map", st);
}

const char *host_ip(ENetAddress *addr) {
	static char str[16];

	/* TODO: pass 16 or 15? */
	enet_address_get_host_ip(addr, str, 16);

	return str;
}

static int intercept(ENetHost *host, ENetEvent *event) {
	ENetBuffer buf;

	(void)event;

#define st ((struct State *)host->peers->data)
	if (host->receivedDataLength == 5 && !memcmp(host->receivedData, "HELLO", 5)) {
		buf.data = "HI";
		buf.dataLength = 2;

		enet_socket_send(host->socket, &host->receivedAddress, &buf, 1);

		/* TODO: prevent dos from slow and plentiful enet connections and from HI */
		LOG("%s:%"PRIu16" says HELLO", host_ip(&host->receivedAddress), host->receivedAddress.port);
		return 1;
	}

	if (host->receivedDataLength == 8 && !memcmp(host->receivedData, "HELLOLAN", 8)) {
		/* TODO: HELLOLAN */
		buf.data = "{}";
		buf.dataLength = 2;

		enet_socket_send(host->socket, &host->receivedAddress, &buf, 1); return 1;

		LOG("%s:%"PRIu16" says HELLOLAN", host_ip(&host->receivedAddress), host->receivedAddress.port);
		return 1;
	}

#undef st

	return 0;
}

void set_funcs_packetrecv(struct State *st);
void set_funcs_event(struct State *st);
void set_funcs_send(struct State *st);
static void set_funcs(struct State *st) {
	st->f.tick = tick;
	st->f.before_log = before_log;
	st->f.after_log = after_log;
	set_funcs_packetrecv(st);
	set_funcs_event(st);
	set_funcs_send(st);
	st->f.get_spawn_position = get_spawn_position;
	st->f.get_spawn_time = get_spawn_time;
	st->f.spawn_player = spawn_player;
	st->f.set_ammo = set_ammo;
	st->f.reload_player = reload_player;
	st->f.set_fog = set_fog;
	st->f.prepare_map_load = prepare_map_load;
	st->f.finish_map_load = finish_map_load;
	st->f.clear_map = clear_map;
	st->f.load_vxl_from_mem = load_vxl_from_mem;
	st->f.load_vxl_from_file = load_vxl_from_file;
	st->f.begin_load_vxl_from_file = begin_load_vxl_from_file;
	st->f.load_map = load_map;
	st->f.finish_cull = fin_cull;
	st->f.block_action_rm = block_action_rm;
	st->f.block_action_cull = block_action_cull;
	st->f.block_action = block_action;
	st->f.set_position = set_position;
	st->f.block_line = block_line;
	st->f.kill = func_kill;
	st->f.get_hit_damage = get_hit_damage;
	st->f.set_hp = set_hp;
	st->f.set_hp_directional = set_hp_directional;
	st->f.damage_player = damage_player;
	st->f.damage_player_directional = damage_player_directional;
	st->f.server_msg = server_msg;
	st->f.player_msg = player_msg;
	st->f.remove_grenade = remove_grenade;
	st->f.detonate_grenade = detonate_grenade;
	st->f.set_block_color = set_block_color;
	st->f.set_jump = set_jump;
	st->f.set_tool = set_tool;
	st->f.tick_player_physics = tick_player_physics;
	st->f.capture_intel = capture_intel;
	st->f.pickup_intel = pickup_intel;
	st->f.drop_intel = drop_intel;
	st->f.restock = restock;
	st->f.move_intel = move_intel;
	st->f.after_player_destroy = after_player_destroy;
	st->f.before_estimated_fire = before_estimated_fire;
	st->f.boot_players_to_limbo = boot_players_to_limbo;
	st->f.demand_fingerprint = demand_fingerprint;
	st->f.move_tent = move_tent;
	st->f.register_grenade = register_grenade;
	st->f.spawn_grenade = spawn_grenade;
	st->f.load_initial_map = load_initial_map;
	st->f.set_orientation = set_orientation;
}

static void set_defaults(struct State *st) {
	fvec3 hidden = {HUGE_VAL, HUGE_VAL, HUGE_VAL};

	st->globals.fog[0] = 255;
	st->globals.fog[1] = 232;
	st->globals.fog[2] = 128;

	LITCPY(st->globals.teamname[0], "Blue");
	LITCPY(st->globals.teamname[1], "Green");

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

#define ARG_GET_UL(name, max) \
	errno = 0; \
	name = strtoul(optarg, &end, 10); \
\
	if (!isdigit(optarg[0]) || name >= max || end != optarg+strlen(optarg)) { \
		fprintf(stderr, #name" must be between 0 and %lu, inclusive.\n", (unsigned long)(max)); \
		exit(EXIT_FAILURE); \
	} \

/* config_path is relative to root_path, root_path defaults to . */
/* TODO: assert(config_path[0] != '/') */
#define USAGE "usage: %s [-c config_path] [-d root_path] [-m mem_limit] [-p udp_port]\n"
static void parse_args(int argc, char **argv) {
	char *end;
	int ch;

	while ((ch = getopt(argc, argv, "c:d:m:p:")) != -1) {switch (ch){
	case 'c':
		cfg = optarg;
		break;
	case 'd':
		root_path = optarg;
		break;
	case 'm':
		ARG_GET_UL(memlimit, RLIM_INFINITY);
		break;
	case 'p':
		/* TODO: if port can be <1024 you had better setuid off of root */
		/* TODO: 0 is reserved */
		ARG_GET_UL(port, 65535);
		break;
	default:
		fprintf(stderr, USAGE, argv[0]);
		exit(EXIT_FAILURE);
	}}
}

static void atexit_server(void) {
	plid i;

	for (i=0;i<MASTERLIST_MAX_PEERS;i++) {
		if (exit_st->ms.host->peers[i].state == ENET_PEER_STATE_CONNECTED)
			enet_peer_disconnect_now(exit_st->ms.host->peers+i, 0);
	}

	masterlist_deinit(&exit_st->ms);

	exit_st->f.on_shutdown(exit_st);
	close_lua();

	for (i=0;i<MAX_PLAYERS;i++) {
		if (exit_st->p[i].connected)
			enet_peer_disconnect_now(exit_st->host->peers+i, 5); /* "Server shutdown" to betterspades and maybe iv of spades */
	}

	free(keepSolid);
	free(rememberedSolidity);
	free(stack.data);
	pvx_destroy_bitmask(&exit_st->globals.map);
	enet_host_destroy(exit_st->host);
	free(exit_st->globals.grenades);
	free(exit_st);
}

static struct State *st_init(void) {
	ENetAddress addr;
	struct State *st;

	st = calloc(1, sizeof(struct State));
	if (st == NULL)
		ERR("calloc");

	st->globals.grenadeSize = 256;
	st->globals.grenadeCount = 0;
	st->globals.grenades = calloc(256, sizeof(struct Grenade));
	if (st->globals.grenades == NULL)
		ERR("calloc");

	st->globals.cullPersonality = CULL_PERSONALITY_OPENSPADES;

	addr.host = ENET_HOST_ANY;
	addr.port = port;

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

	if (pvx_create_bitmask(&st->globals.map, 512, 512, 64) != 0)
		ERR("pvx_create_bitmask");

	if (masterlist_init(&st->ms) != 0)
		ERR("masterlist_init");

	st->ms.port = addr.port;
	st->ms.maxplayers = MAX_PLAYERS;

	return st;
}

static void limit_mem(rlim_t bytes) {
	struct rlimit rl;
	rl.rlim_cur = bytes;
	rl.rlim_max = bytes;

	setrlimit(RLIMIT_DATA, &rl);
}

static void sig_handler(int sig) {
	(void)sig;
	keepRunning = 0;
}

void hook_textcodec_late(struct State *st);
void hook_textcodec_early(struct State *st);
int main(int argc, char **argv) {
	struct State *st;

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	parse_args(argc, argv);

	if (root_path && chdir(root_path) != 0)
		ERR("chdir");
	if (memlimit)
		limit_mem(memlimit);

	sandbox();

	st = st_init();

	if (init_cull_stack(stackData) != 0)
		ERR("malloc");
	if ((rememberedSolidity = calloc(1, 512*512*sizeof(uint64_t))) == NULL)
		ERR("calloc");
	if ((keepSolid = calloc(1, 512*512*sizeof(uint64_t))) == NULL)
		ERR("calloc");

	exit_st = st;
	if (atexit(atexit_server)) {
		fputs("atexit register failure\n", stderr);
		exit(EXIT_FAILURE);
	}

	hook_textcodec_late(st);
	hook_lua(cfg, port, st);
	hook_textcodec_early(st);

	st->f.load_initial_map(st);

	setpanic_lua();
	while (keepRunning)
		do_loop(st);

	exit(EXIT_SUCCESS);
}
