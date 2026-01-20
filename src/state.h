#ifndef LS2_SERVER_STATE_H
#define LS2_SERVER_STATE_H
#include <enet/enet.h>
#include "protocol.h"
#include "bitmask.h"
#include "masterlist.h"

/* For blocks and block lines when no player has placed them */
#define PID_COLOR_ANONYMOUS 32
/* TODO: merge broadcast and broadcast_except? */
#define PID_SET_FLAG(x) ((uint32_t)(x) << 29)
#define PID_BROADCAST PID_SET_FLAG(1)
#define PID_BROADCAST_EXCEPT(pid) ((plid)((uint32_t)(pid) | PID_SET_FLAG(2)))
#define PID_BROADCAST_TEAM(team) ((plid)((uint32_t)(team) | PID_SET_FLAG(3)))
/* Quite the mouthful. */
#define PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(team, pid) ((plid)((uint32_t)(pid) << 8 | (uint32_t)(team) | PID_SET_FLAG(4)))
#define PID_BROADCAST_EXCEPT_TEAM(team) PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(team, MAX_PLAYERS)

/* Type used for storing time as nanoseconds (clock) */
typedef uint64_t clk;

/* Type used for internally storing player IDs -- not sent over the network, that's a uint8_t */
typedef int32_t plid;

/* [0] is B, [1] is G, [2] is R */
typedef uint8_t color[3];

/* An int that should be treated as boolean. */
typedef int bint;

/* Vector of 3 signed ints, to represent position.
 * Why signed? Sending a GrenadeDestroy at -1 is perfectly valid -- it just affects things at 0.
 */
/*typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
} ivec3;

typedef struct {
	float x;
	float y;
	float z;
} fvec3;*/

typedef ivec3p ivec3;
typedef fvec3p fvec3;

/* TODO: aoscam/src/demoncore.h has an incompatible struct definition; i recommend merging all the random struct Player's strewn about the place. Or just removing everything after int joined; */
struct Player {
	fvec3 pos;
	fvec3 ori;
	fvec3 vel;
	uint8_t inputs;
	uint8_t mouseInputs;
	uint8_t tool;
	int wade;
	int airborne;
	uint8_t weapon;
	int joined;
	int alive;
	char name[16];
	uint8_t team;
	uint8_t blockColor[3];
	int hp;
	uint32_t score; /* TODO: pretty sure the clients all use an int32_t */
	fvec3 lastagreedpos;
	/* team and weapon are set to these two on the next spawn */
	uint8_t newteam;
	uint8_t newweapon;
	clk spawntime;
	uint8_t blocks;
	uint8_t grenades;
};

struct State;
struct Functions {
	/*
	 * Events
	 */
	/* This one should disconnect the player if server full, banned, etc. */
	void (*on_any_connect)(plid pid, struct State *st);
	/* This one gets called if the player was not kicked after connecting. */
	void (*on_successful_connect)(plid pid, struct State *st);
	void (*on_disconnect)(plid pid, struct State *st);

	/* Handles received packets of any kind -- valid or invalid. Returns nonzero if packet should be marked as crap. */
	int (*on_any_packet)(plid pid, ENetPacket *packet, struct State *st);
	/* Handles received packets that are known and valid */
	void (*on_sane_packet)(plid pid, ENetPacket *packet, struct State *st);
	/* Handles received packets that are unknown or invalid */
	void (*on_crap_packet)(plid pid, ENetPacket *packet, struct State *st);

	void (*on_join)(plid pid, unsigned team, unsigned weapon, const char *name, struct State *st);
	void (*on_switch)(plid pid, unsigned team, unsigned weapon, struct State *st);

	void (*on_position)(plid pid, fvec3 pos, struct State *st);
	void (*on_orientation)(plid pid, fvec3 ori, struct State *st);
	void (*on_move_input)(plid pid, unsigned bitmask, struct State *st);
	void (*on_mouse_input)(plid pid, unsigned bitmask, struct State *st);
	void (*on_color_change)(plid pid, color color, struct State *st);
	void (*on_block_action)(plid pid, ivec3 pos, unsigned type, struct State *st);
	void (*on_block_line)(plid pid, ivec3 start, ivec3 end, struct State *st);
	void (*on_chat)(plid pid, const char *msg, unsigned type, struct State *st);
	void (*on_tool_change)(plid pid, unsigned tool, struct State *st);
	void (*on_hit)(plid pid, unsigned type, plid hitPlayer, struct State *st);
	void (*on_grenade)(plid pid, fvec3 pos, fvec3 vel, float fuse, struct State *st);
	void (*on_reload)(plid pid, unsigned mag, unsigned reserve, struct State *st);

	/* TODO: player spawn? why not just spawn */
	fvec3 (*on_player_spawn)(plid pid, struct State *st);
	clk (*on_kill)(plid pid, struct State *st);
	int (*get_hit_damage)(plid pid, unsigned type, struct State *st);
	/* TODO: make name less ambiguous? refers to players dying/disconnecting/whatever but could be interpreted as block destroying */
	void (*after_player_destroy)(plid pid, struct State *st);
	void (*on_game_end)(struct State *st);
	void (*on_shutdown)(struct State *st);
	/* TODO: just have "log" */
	void (*before_log)(struct State *st);
	void (*after_log)(struct State *st);

	/*
	 * Actions -- set pid to PID_BROADCAST to broadcast to all players,
	 * and to PID_BROADCAST_EXCEPT(pid) to broadcast to all players except for pid
	 * and to PID_BROADCAST_TEAM(team) to broadcast to all players on that team
	 */
	void (*tick)(struct State *st);

	/*
	 * Global actions -- these'll update the global state and broadcast to all players
	 */

	/* vxl, scientists hypothesize pvx may one day become available */
	void (*load_initial_map)(struct State *st);
	void (*clear_map)(struct State *st);
	void (*prepare_map_load)(struct State *st);
	void (*finish_map_load)(struct State *st);
	void (*load_map_from_file)(const char *path, struct State *st);
	void (*finish_cull)(struct State *st);
	uint32_t (*block_action_rm)(ivec3 pos, unsigned type, plid from, struct State *st);
	void (*block_action_cull)(ivec3 pos, uint32_t mask, struct State *st);
	void (*block_action)(ivec3 pos, unsigned type, plid from, struct State *st);
	/* Try to limit sent block lines to 50 blocks or openspades will eat you. */
	void (*block_line)(ivec3 start, ivec3 end, plid from, struct State *st);
	void (*set_fog)(color color, struct State *st);
	void (*tick_player_physics)(plid pid, float timeDelta, struct State *st);
	void (*detonate_grenade)(size_t index, struct State *st);
	void (*boot_players_to_limbo)(struct State *st);
	void (*send_grenade)(plid pid, fvec3 pos, fvec3 vel, float fuse, plid from, struct State *st);
	size_t (*register_grenade)(plid pid, unsigned team, fvec3 pos, fvec3 vel, float fuse, struct State *st);
	size_t (*spawn_grenade)(plid pid, unsigned team, fvec3 pos, fvec3 vel, float fuse, struct State *st);

	/*
	 * Player actions
	 */
	int (*send_packet)(plid pid, const void *data, size_t length, struct State *st);
	int (*send_packet_unreliable)(plid pid, const void *data, size_t length, struct State *st);
	void (*send_map)(plid pid, struct State *st);
	void (*send_state)(plid pid, struct State *st);
	void (*send_state_ctf)(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st);
	void (*send_state_tc)(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, unsigned tentcount, const fvec3 *tentpos, unsigned *tentteam, struct State *st);
	void (*send_connected_players)(plid pid, struct State *st);
	void (*spawn_player)(plid pid, struct State *st);
	/* TODO: figure out how to log chat properly -- just on send_chat? */
	/* also, what about logging /login? */
	void (*send_chat)(plid pid, const char *msg, unsigned type, plid from, struct State *st);
	void (*send_block_action)(plid pid, ivec3 pos, unsigned type, plid from, struct State *st);
	/* Try to limit sent block lines to 50 blocks or openspades will eat you. */
	void (*send_block_line)(plid pid, ivec3 start, ivec3 end, plid from, struct State *st);
	void (*send_set_color)(plid pid, color color, plid from, struct State *st);
	void (*send_player_update)(plid pid, struct State *st); /* TODO: hide too-far players, /ups */
	void (*send_orientation)(plid pid, fvec3 ori, struct State *st);
	void (*send_position)(plid pid, fvec3 pos, struct State *st);
	void (*send_reload)(plid pid, unsigned mag, unsigned reserve, plid from, struct State *st);
	void (*send_intel_capture)(plid pid, bint winning, plid from, struct State *st);
	void (*send_intel_pickup)(plid pid, plid from, struct State *st);
	void (*send_intel_drop)(plid pid, fvec3 pos, plid from, struct State *st);
	void (*send_restock)(plid pid, plid from, struct State *st);
	void (*send_move_object)(plid pid, fvec3 pos, unsigned id, unsigned team, struct State *st);
	void (*send_map_start)(plid pid, unsigned size, struct State *st);
	void (*send_fog)(plid pid, color color, struct State *st);

	void (*restock)(plid pid, struct State *st);
	/* TODO: allow hijacking respawn time */
	void (*kill)(plid pid, unsigned type, plid killer, struct State *st);
	void (*set_ammo)(plid pid, unsigned mag, unsigned reserve, struct State *st);
	/* Negative HP is sent and handled as 0 and >255 is sent as 255. */
	void (*set_hp)(plid pid, int hp, struct State *st);
	void (*set_hp_directional)(plid pid, int hp, fvec3 pos, struct State *st);
	void (*set_tool)(plid pid, unsigned tool, struct State *st);
	/* TODO: how to handle anonymous? */
	void (*set_color)(plid pid, color color, struct State *st);
	void (*set_position)(plid pid, fvec3 pos, struct State *st);
	void (*set_orientation)(plid pid, fvec3 ori, struct State *st);
	void (*set_jump)(plid pid, struct State *st);
	void (*capture_intel)(plid pid, bint winning, struct State *st);
	void (*pickup_intel)(plid pid, struct State *st);
	void (*drop_intel)(plid pid, fvec3 pos, struct State *st);
	void (*move_intel)(unsigned team, fvec3 pos, struct State *st);
	void (*move_tent)(unsigned team, fvec3 pos, struct State *st);
};

struct Globals {
	struct BitmaskUData map;
	int loadingMap;
	struct Grenade *grenades;
	size_t grenadeSize;
	size_t grenadeCount;
	color fog;
	color teamcolor[2];
	char teamname[2][10];
	unsigned teamscore[2];
	unsigned maxscore;
	plid intelplayers[2];
	fvec3 intelpos[2];
	fvec3 tentpos[2];
};

struct State {
	ENetHost *host;
	struct Player p[256];
	struct Globals globals;
	struct Functions f;
	struct MasterState ms;
	/* default map */
	clk epoch; /* Time the server was started at, as measured by get_time() */
	clk nextTickTime;
	clk tickrate;
	int crapline;
	const char *crapcond;
	const char *crappacketname;
};
#endif
