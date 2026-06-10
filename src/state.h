#ifndef LS2_SERVER_STATE_H
#define LS2_SERVER_STATE_H
#include <enet/enet.h>
#include "protocol.h"
#include "bitmask.h"
#include "masterlist.h"

/* Some printf format constants */
#include <inttypes.h>
#define PRIuSIZET "zu"
#define PRIiPID "i"

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
/* Also accepts broadcast pids */
typedef plid bplid;
/* Gets sent over the wire, use whatever value you want as long as the client understands it and it fits in a uint8_t */
typedef plid nplid;

/* Holds a team value -- what else did you expect? */
typedef unsigned teamid;

/* Holds an inGame team value -- i.e., not spectators */
typedef unsigned gteamid;

/* Gets sent over the wire, use whatever.
 * In Lua land, it is 1-indexed, not 0-indexed, i.e. its range maps from 1,256 -> 0,255 inclusive.
 * C uses standard 0-indexing for this.
 */
typedef unsigned nteamid;

/* [0] is B, [1] is G, [2] is R */
typedef uint8_t color[3];

/* An int that should be treated as boolean. */
typedef int bint;

/* Vector of 3 signed ints, to represent position.
 * Why signed? Sending a GrenadeDestroy at -1 is perfectly valid -- it just affects things at 0.
 */
typedef ivec3p ivec3;
typedef fvec3p fvec3;

/* Work around buggerspades bugs */
#define BS_BUG_INFLOOR (1 << 0)
#define BS_BUG_NOSHORTPLAYER (1 << 1)
#define BS_BUG_SCREWED_DISCONNECT_DATA (1 << 2)

/* OpenSpades CP-437 takes 0x0a as lf, 0x0d as cr, beta as sharp s, gamma as tua */
#define QUIRK_OS_CP437 (1 << 3)

/* These ones may send either CP-437, or UTF-8 if prefixed with '\xff';
 * They will also unconditionally be sent UTF-8 messages to hammer out
 * deficiencies in shitty CP-437 codecs.
 */
#define QUIRK_UTF8 (1 << 4)

/* BetterSpades doesn't understand UTF-8 *or* CP-437, so messages sent to it
 * must be transcoded into ASCII.
 */
#define QUIRK_ASCII (1 << 5)

/* Voxlap and BetterSpades cull floating voxels for spade 3x and grenade on
 * all sides of the shape, even on sides with no adjacent removed solid voxel.
 * OpenSpades only culls where voxels have been removed.
 *
 * Did that make sense?
 */
#define QUIRK_OS_BACTION_CULL (1 << 6)

/* IV of Spades and ZeroSpades move the Gray color to '\x08' from '\x07' for
 * some reason and put an image-prefix-code-thing in its place.
 */
#define QUIRK_UTF8_COLOR_IMG (1 << 7)

/* ZeroSpades decided CreatePlayer should be off by 2.4 instead of just 2
 * (commit 1675f07f81327a97b180f76f19bb67f801c3c8ce)
 */
#define QUIRK_INSKY (1 << 8)

/* TODO: BS_BUG_NODEADNADE */
/* TODO: BS_BUG_BORKEDRELOAD */
/* TODO: BS_BUG_MOUSEINPUTISFUCKED */
/* TODO: BS_BUG_INCOMPATIBLE_CHAT_STANDARD */
/* TODO: wonder how to handle sprintcrouching */

/* TODO: consider the version stuff an ext too? */
/* TODO: consider the *other* version packet an ext? */
/* TODO: do i have to bother with that mapCached thing? */
/* TODO: EXT_COMPATIBLE_CHAT_STANDARD */
/* TODO: EXT_76WUPD */
/* TODO: EXT_BMASKUPD -- for this one you MUST have some way to clear the velocity of a player so you don't have to send redundant data if the velocity gets desynchronized */
/* TODO: combine the latter two, or go to 10 Hz? */
/* TODO: EXT_12HzPOSRATE */
/* TODO: EXT_60HzPOSRATE */
/* TODO: EXT_csUTF8 */
/* Like chat macros but superpowered. . . should probably allow using them as fallback though */
/* TODO: EXT_BINDKEYS */
/* With my little research into how ENet deals with its datagrams I
 * think it should be possible for the client to send some early data
 * down the wire, before map transfer does anything
 * TODO: get libpvx2 into a usable state already
 */
/* TODO: EXT_PVX */
/* At least the stats */
/* TODO: EXT_CUSTOMGUN */
/* TODO: go find your scattered notes for that gamma protocol */
/* TODO: EXT_BS_PLAYERPROP */
/* TODO: EXT_PUBKEY_AUTHN -- should this one be handled more generically and by lua? */
/* TODO: play with enet channels -- all clients i've looked into have
 * a max of 1, but we can change that for at least my private client
 * TODO: play with alternate transports, libenetproto
 */

/* TODO: aoscam/src/demoncore.h has an incompatible struct definition; i recommend merging all the random struct Player's strewn about the place. Or just removing everything after int joined; */
/* TODO: the capitalization is inconsistent here */
struct Player {
	/* Or NULL if there is no attached ENet peer -- packets just get sent to /dev/null unless overridden */
	ENetPeer *peer;
	plid pid;

	/*
	 * Life-based
	 */
	clk estfiretime; /* Used to decrease estMagAmmo and not much else. . . TODO: buggerspades */
	clk reloadtime;
	fvec3 pos;
	fvec3 ori;
	fvec3 vel;
	unsigned reserveAmmo;
	int alive;
	int wade;
	int airborne;
	uint8_t blockColor[3];
	uint8_t inputs;
	uint8_t mouseInputs;
	uint8_t tool;
	uint8_t gun;
	uint8_t team;
	uint8_t blocks;
	uint8_t grenades;
	/* TODO: validate pellets -- or leave that to dd? */
	uint8_t estMagAmmo; /* Estimated magazine ammo -- what we *think* this player has */
	uint8_t maxMagAmmo; /* Max magazine ammo -- the most this player can physically have */

	/*
	 * Join-based
	 */
	char name[16];
	clk spawntime;
	fvec3 lastagreedpos;
	uint32_t score; /* TODO: pretty sure the clients all use an int32_t */
	int joined;
	int hp;
	/* team and gun are set to these two on the next spawn */
	uint8_t newteam;
	uint8_t newgun;

	/*
	 * Connection-based
	 */
	uint8_t idChar;
	uint8_t verMajor;
	int connected;
	int initStateSent;
	uint64_t bugMask;
	uint64_t extMask;
	/* 0 if unwanted, 1 if only verExt wanted, 2 if only ver wanted, 3 if handshake or ver wanted --
	 * it starts at 3, then becomes 2 or 1, then becomes 1, then becomes 0.
	 */
	int wantFingerprint;
	/* TODO: is checking for handshake whatnot really necessary? */
	int handshaked;
	int hasverext;
	uint8_t verMinor;
	uint8_t verPatch;
	uint8_t verExtMajor;
	uint8_t verExtMinor;
	uint32_t verExtFlags;
	uint8_t verExtPatch;
	char verMsg[256];
	char verExtCliName[16];
	char verExtLang[6];
};

struct State;
struct Functions {
	/*
	 * Events
	 * TODO: reorganize the entire thing, then split up into even more individual files
	 */
	plid (*assign_new_pid)(struct State *st);
	/* This one should disconnect the player if server full, banned, etc. */
	void (*on_any_connect)(nplid pid, struct State *st);
	/* This one gets called if the player was not kicked after connecting. */
	void (*on_successful_connect)(plid pid, struct State *st);
	void (*on_disconnect)(plid pid, struct State *st);

	/* Handles received packets of any kind -- valid or invalid. Returns nonzero if packet should be marked as crap. */
	int (*on_any_packet)(plid pid, const void *data, size_t length, struct State *st);
	/* Handles received packets that are known and valid */
	void (*on_sane_packet)(plid pid, const void *data, size_t length, struct State *st);
	/* Handles received packets that are unknown or invalid */
	void (*on_crap_packet)(plid pid, const void *data, size_t length, struct State *st);

	void (*on_join)(plid pid, teamid team, unsigned gun, const char *name, struct State *st);
	void (*on_switch)(plid pid, teamid team, unsigned gun, struct State *st);

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
	void (*on_reload)(plid pid, struct State *st);
	void (*on_handshake)(plid pid, struct State *st);
	void (*on_version)(plid pid, unsigned idChar, unsigned major, unsigned minor, unsigned patch, const char *msg, size_t msglen, struct State *st);
	void (*on_version_ext)(plid pid, unsigned major, unsigned minor, unsigned patch, uint32_t flags, const char *cli, size_t clilen, const char *lang, size_t langlen, struct State *st);

	fvec3 (*get_spawn_position)(plid pid, struct State *st);
	clk (*get_spawn_time)(plid pid, struct State *st);
	int (*get_hit_damage)(plid pid, unsigned type, struct State *st);
	plid (*get_effective_max_players)(struct State *st);
	plid (*get_anon_pid)(struct State *st);
	/* TODO: make name less ambiguous? refers to players dying/disconnecting/whatever but could be interpreted as block destroying */
	void (*after_player_destroy)(plid pid, struct State *st);
	void (*before_estimated_fire)(plid pid, struct State *st);
	void (*on_game_end)(struct State *st);
	void (*on_shutdown)(struct State *st);
	/* TODO: just have "log" */
	void (*before_log)(struct State *st);
	void (*after_log)(struct State *st);
	void (*on_masterlist_successful_connect)(uint32_t peerid, struct State *st);
	void (*on_masterlist_reconnect_attempt)(uint32_t peerid, struct State *st);
	void (*on_masterlist_disconnect)(uint32_t peerid, struct State *st);

	/*
	 * Actions -- set pid to PID_BROADCAST to broadcast to all players,
	 * and to PID_BROADCAST_EXCEPT(pid) to broadcast to all players except for pid
	 * and to PID_BROADCAST_TEAM(team) to broadcast to all players on that team
	 */
	void (*tick)(struct State *st);

	/*
	 * Global actions -- these'll update the global state and broadcast to all players
	 */

	void (*load_initial_map)(struct State *st);
	void (*clear_map)(struct State *st);
	void (*prepare_map_load)(struct State *st);
	void (*finish_map_load)(struct State *st);
	/* scientists hypothesize pvx may one day become available */
	int (*load_vxl_from_mem)(const void *data, size_t len, struct State *st);
	int (*load_vxl_from_file)(const char *path, struct State *st);
	int (*begin_load_vxl_from_file)(const char *path, struct State *st);
	int (*load_map)(const char *name, struct State *st);
	void (*finish_cull)(struct State *st);
	uint32_t (*block_action_rm)(ivec3 pos, unsigned type, nplid from, struct State *st);
	void (*block_action_cull)(ivec3 pos, uint32_t mask, struct State *st);
	void (*block_action)(ivec3 pos, unsigned type, nplid from, struct State *st);
	/* Try to limit sent block lines to 50 blocks or openspades will eat you. */
	void (*block_line)(ivec3 start, ivec3 end, nplid from, struct State *st);
	void (*set_fog)(color color, struct State *st);
	void (*tick_player_physics)(bplid pid, float timeDelta, struct State *st);
	void (*remove_grenade)(size_t index, struct State *st);
	void (*detonate_grenade)(size_t index, struct State *st);
	void (*boot_players_to_limbo)(struct State *st);
	size_t (*register_grenade)(plid pid, teamid team, fvec3 pos, fvec3 vel, clk fuse, struct State *st);
	size_t (*spawn_grenade)(plid pid, teamid team, fvec3 pos, fvec3 vel, clk fuse, struct State *st);
	void (*server_msg)(bplid pid, const char *msg, struct State *st);
	void (*player_msg)(const char *msg, unsigned type, plid from, struct State *st);

	/*
	 * Player actions
	 */

	/*
	 * Player send functions -- less-hazardous alternatives to manually constructing packets
	 */
	int (*send_packet)(bplid pid, const void *data, size_t length, struct State *st);
	int (*send_packet_unreliable)(bplid pid, const void *data, size_t length, struct State *st);

	void (*send_map)(bplid pid, struct State *st);
	/* TODO: send_state feels more like it belongs as a "player func" than a "send function" */
	void (*send_state)(plid pid, struct State *st);

	void (*send_state_ctf)(bplid pid, nplid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st);
	void (*send_state_tc)(bplid pid, nplid from, const char teamname[][10], const color *teamcolor, color fog, unsigned tentcount, const fvec3 *tentpos, unsigned *tentteam, struct State *st);
	void (*send_connected_players)(bplid pid, struct State *st);
	/* TODO: figure out how to log chat properly -- just on send_chat? */
	/* also, what about logging /login? */
	void (*send_chat)(bplid pid, const char *msg, unsigned type, nplid from, struct State *st);
	void (*send_block_action)(bplid pid, ivec3 pos, unsigned type, nplid from, struct State *st);
	/* Try to limit sent block lines to 50 blocks or openspades will eat you. */
	void (*send_block_line)(bplid pid, ivec3 start, ivec3 end, nplid from, struct State *st);
	void (*send_set_block_color)(bplid pid, color color, nplid from, struct State *st);
	void (*send_set_tool)(bplid pid, unsigned tool, plid from, struct State *st);
	void (*send_player_update)(bplid pid, struct State *st); /* TODO: hide too-far players */
	void (*send_orientation)(bplid pid, fvec3 ori, struct State *st);
	void (*send_position)(bplid pid, fvec3 pos, struct State *st);
	void (*send_reload)(bplid pid, unsigned mag, unsigned reserve, nplid from, struct State *st);
	void (*send_intel_capture)(bplid pid, bint winning, nplid from, struct State *st);
	void (*send_intel_pickup)(bplid pid, nplid from, struct State *st);
	void (*send_intel_drop)(bplid pid, fvec3 pos, nplid from, struct State *st);
	void (*send_restock)(bplid pid, nplid from, struct State *st);
	void (*send_disconnect)(bplid pid, nplid from, struct State *st);
	/* TODO: should this be gteamid or something else entirely? */
	void (*send_move_object)(bplid pid, fvec3 pos, unsigned id, gteamid team, struct State *st);
	void (*send_map_start)(bplid pid, unsigned size, struct State *st);
	void (*send_fog)(bplid pid, color color, struct State *st);
	void (*send_existing_player)(bplid pid, unsigned team, unsigned gun, unsigned tool, unsigned score, color blockColor, const char *name, plid from, struct State *st);
	void (*send_move_input)(bplid pid, unsigned inputs, plid from, struct State *st);
	void (*send_mouse_input)(bplid pid, unsigned inputs, plid from, struct State *st);
	void (*send_kill)(bplid pid, clk spawndelta, unsigned type, plid killer, plid from, struct State *st);
	void (*send_grenade)(bplid pid, fvec3 pos, fvec3 vel, float fuse, nplid from, struct State *st);
	void (*send_spawn_player)(bplid pid, fvec3 pos, unsigned gun, nteamid team, const char *name, plid from, struct State *st);

	/*
	 * Player funcs -- these send packets and modify player state
	 */
	void (*spawn_player)(plid pid, fvec3 pos, struct State *st);
	void (*reload_player)(plid pid, struct State *st);
	void (*restock)(plid pid, struct State *st);
	void (*demand_fingerprint)(plid pid, struct State *st);
	/* TODO: allow hijacking respawn time */
	void (*kill)(plid pid, unsigned type, plid killer, struct State *st);
	void (*set_ammo)(plid pid, unsigned mag, unsigned reserve, struct State *st);
	/* Negative HP is sent and handled as 0 and >255 is sent as 255. */
	void (*set_hp)(plid pid, int hp, struct State *st);
	void (*set_hp_directional)(plid pid, int hp, fvec3 pos, struct State *st);
	void (*damage_player)(plid pid, int hp, unsigned type, plid damager, struct State *st);
	void (*damage_player_directional)(plid pid, int hp, fvec3 pos, unsigned type, plid damager, struct State *st);
	void (*set_tool)(plid pid, unsigned tool, struct State *st);
	/* TODO: how to handle anonymous? */
	void (*set_block_color)(plid pid, color color, struct State *st);
	void (*set_position)(plid pid, fvec3 pos, struct State *st);
	void (*set_orientation)(plid pid, fvec3 ori, struct State *st);
	void (*set_jump)(plid pid, struct State *st);
	void (*capture_intel)(plid pid, bint winning, struct State *st);
	void (*pickup_intel)(plid pid, struct State *st);
	void (*drop_intel)(plid pid, fvec3 pos, struct State *st);
	void (*move_intel)(gteamid team, fvec3 pos, struct State *st);
	void (*move_tent)(gteamid team, fvec3 pos, struct State *st);
};

#define CULL_PERSONALITY_VOXLAP 0
#define CULL_PERSONALITY_OPENSPADES 1

struct Globals {
	struct BitmaskUData map;
	/* pristineBuf points to some zlib-compressed map data if:
	 * the current map was loaded with load_map(),
	 * a PATH.zlib file existed at that time,
	 * and the map hasn't been touched with e.g. block_action() since it was first loaded.
	 * Otherwise it'll be NULL.
	 * The rules for this'll probably change later, if it hasn't been 10 years yet.
	 */
	void *pristineBuf;
	size_t pristineLen;
	struct Grenade *grenades;
	size_t grenadeSize;
	size_t grenadeCount;
	char teamname[2][10];
	unsigned teamscore[2];
	unsigned maxscore;
	int loadingMap;
	int cullPersonality;
	plid intelplayers[2];
	fvec3 intelpos[2];
	fvec3 tentpos[2];
	color fog;
	color teamcolor[2];
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
	const char *crapcond;
	const char *crappacketname;
	int crapline;
	int crapsilence;
};
#endif
