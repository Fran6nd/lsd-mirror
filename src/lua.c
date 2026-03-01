#include <stdio.h>
#include <math.h>
#include <arpa/inet.h>
#include <errno.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lualib.h>
#include "state.h"
#include "demoncore.h"
#include <poll.h>

#define LOG(x, ...) fprintf(stderr, x"\n", __VA_ARGS__)
#define LOG1(x) fputs(x"\n", stderr);
#define LERR luaL_error
#define CBAIL(x, ...) do {LOG(x, __VA_ARGS__); return;} while (0)
#define CBAILN1(x, ...) do {LOG(x, __VA_ARGS__); return -1;} while (0)
#define CBAIL1N1(x, ...) do {LOG1(x); return -1;} while (0)

static lua_State *l;

/* TODO: color -> struct */
void get_color2(lua_State *l, int table, color color) {
	lua_pushliteral(l, "b");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[0] = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "g");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[1] = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "r");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[2] = lua_tonumber(l, -1);
	lua_pop(l, 1);
}

/* TODO: replace set_color, block_action and PID_COLOR_ANONYMOUS with dedicated bulk build/destroy api */
plid check_plid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val < 0 || val >= MAX_PLAYERS)
		LERR(l, "plid is invalid (%.0f)", val);
	return val;
}

bplid check_bplid(lua_State *l, int numArg) {
	return luaL_checknumber(l, numArg);
}

bplid check_nplid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val < 0 || val >= 256)
		LERR(l, "nplid is invalid (%.0f)", val);
	return val;
}

unsigned check_teamid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val != 0 && val != 1 && val != 255)
		LERR(l, "teamid is invalid (%.0f)", val);
	return val;
}

unsigned check_gteamid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val != 0 && val != 1)
		LERR(l, "gteamid is invalid (%.0f)", val);
	return val;
}

/* TODO: handle legitimate use cases for negative pos (i.e. -1), ***don't overflow the int -- set to -1 if you have to*** */
ivec3 get_ivec3(lua_State *l, int table, int positiveZ) {
	ivec3 val;
	double num;

	lua_pushliteral(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3.x should be a number");

	num = floor(lua_tonumber(l, -1));
	lua_pop(l, 1);
	if (num < 0 || num >= 512)
		LERR(l, "ivec3 is out of bounds (x=%.0f)", num);
	val.x = num;


	lua_pushliteral(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3.y should be a number");

	num = floor(lua_tonumber(l, -1));
	lua_pop(l, 1);
	if (num < 0 || num >= 512)
		LERR(l, "ivec3 is out of bounds (y=%.0f)", num);
	val.y = num;


	lua_pushliteral(l, "z");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3.z should be a number");

	num = floor(lua_tonumber(l, -1));
	lua_pop(l, 1);
	if ((positiveZ && num < 0) || num >= 64)
		LERR(l, "ivec3 is out of bounds (z=%.0f)", num);
	val.z = num;

	return val;
}

fvec3 get_fvec3(lua_State *l, int table) {
	fvec3 val;

	lua_pushliteral(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3.x should be a number");

	val.x = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3.y should be a number");

	val.y = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "z");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3.z should be a number");

	val.z = lua_tonumber(l, -1);
	lua_pop(l, 1);

	return val;
}

static void push_ivec3(ivec3 vec) {
	lua_newtable(l);
	lua_pushlstring(l, "x", 1);
	lua_pushnumber(l, vec.x);
	lua_settable(l, -3);

	lua_pushlstring(l, "y", 1);
	lua_pushnumber(l, vec.y);
	lua_settable(l, -3);

	lua_pushlstring(l, "z", 1);
	lua_pushnumber(l, vec.z);
	lua_settable(l, -3);
}

static void push_fvec3(fvec3 vec) {
	lua_newtable(l);
	lua_pushlstring(l, "x", 1);
	lua_pushnumber(l, vec.x);
	lua_settable(l, -3);

	lua_pushlstring(l, "y", 1);
	lua_pushnumber(l, vec.y);
	lua_settable(l, -3);

	lua_pushlstring(l, "z", 1);
	lua_pushnumber(l, vec.z);
	lua_settable(l, -3);
}

/* TODO: struct it */
static void push_color(const color color) {
	lua_newtable(l);
	lua_pushlstring(l, "b", 1);
	lua_pushnumber(l, color[0]);
	lua_settable(l, -3);

	lua_pushlstring(l, "g", 1);
	lua_pushnumber(l, color[1]);
	lua_settable(l, -3);

	lua_pushlstring(l, "r", 1);
	lua_pushnumber(l, color[2]);
	lua_settable(l, -3);
}

const char *get_team_name_cfg(lua_State *l, unsigned team) {
	const char *result;

	lua_pushnumber(l, team);
	lua_gettable(l, -2);

	if (!lua_isstring(l, -1))
		LERR(l, "team_name should have 2 strings in it");

	result = lua_tostring(l, -1);
	lua_pop(l, 1);

	return result;
}

void read_config_values(lua_State *l, struct State *st) {
	return;
	lua_getglobal(l, "fog");
	if (!lua_istable(l, -1))
		LERR(l, "fog should be a table");

	get_color2(l, -2, st->globals.fog);
	lua_settop(l, 0);


	lua_getglobal(l, "team_name");
	if (!lua_istable(l, -1))
		LERR(l, "team_name should be a table");

	memset(st->globals.teamname[0], 0, 10);
	strncpy(st->globals.teamname[0], get_team_name_cfg(l, 1), 9);

	memset(st->globals.teamname[1], 0, 10);
	strncpy(st->globals.teamname[1], get_team_name_cfg(l, 2), 9);
	lua_settop(l, 0);


	lua_getglobal(l, "max_score");
	if (!lua_isnumber(l, -1))
		LERR(l, "max_score should be a number");

	st->globals.maxscore = lua_tonumber(l, -1);
	lua_settop(l, 0);


	lua_getglobal(l, "team_color");
	if (!lua_istable(l, -1))
		LERR(l, "team_color should be a table");

	lua_pushnumber(l, 1);
	lua_gettable(l, -2);
	if (!lua_istable(l, -1))
		LERR(l, "team_color should have 2 tables in it");

	get_color2(l, -2, st->globals.teamcolor[0]);
	lua_pop(l, 1);

	lua_pushnumber(l, 2);
	lua_gettable(l, -2);
	if (!lua_istable(l, -1))
		LERR(l, "team_color should have 2 tables in it");

	get_color2(l, -2, st->globals.teamcolor[1]);
	lua_settop(l, 0);
}

static struct State *st;
static struct Functions f;
#include "luaawk.h"

static int lsend_state_ctf(lua_State *l) {
	size_t i;
	bplid pid = check_bplid(l, 1);
	nplid from = check_nplid(l, 2);
	char teamname[2][10];
	color teamcolor[2];
	color fog;
	unsigned teamscore[2];
	unsigned maxscore = luaL_checknumber(l, 7);
	plid holders[2] = {-1, -1};
	fvec3 intelpos[2];
	fvec3 tentpos[2];

	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_gettable(l, 3);
		if (!lua_isstring(l, -1))
			LERR(l, "teamname should have 2 strings in it");

		strncpy(teamname[i], lua_tostring(l, -1), 9);
		teamname[i][9] = '\0';
		lua_pop(l, 1);
	}

	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_gettable(l, 4);
		/* TODO: should i use this for color and fvec3? */
		/*if (!lua_istable(l, -1))
			LERR(l, "teamcolor should have 2 tables in it");*/

		get_color2(l, -2, teamcolor[i]); /* TODO: why is this -2? REASON: internal fuckery */
		lua_pop(l, 1);
	}

	get_color2(l, 5, fog);

	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_gettable(l, 6);
		if (!lua_isnumber(l, -1))
			LERR(l, "teamscore should have 2 numbers in it");
		teamscore[i] = lua_tonumber(l, -1);
		lua_pop(l, 1);
	}

	/* TODO: what if one player holds both intels */
	/* TODO: should ordering of player args matter? probably */
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_gettable(l, 8);
		/* TODO: should be nil or false? change in the c func too */
		if (lua_isnil(l, -1)) {
			intelpos[i].x = HUGE_VAL;
			intelpos[i].y = HUGE_VAL;
			intelpos[i].z = HUGE_VAL;
		} else if (lua_isnumber(l, -1)) {
			holders[i] = lua_tonumber(l, -1);
		} else if (lua_istable(l, -1)) {
			intelpos[i] = get_fvec3(l, -2);
		} else
			LERR(l, "intelloc should have 2 nil, number or table in it");
		lua_pop(l, 1);
	}

	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_gettable(l, 9);
		if (lua_isnil(l, -1)) {
			tentpos[i].x = HUGE_VAL;
			tentpos[i].y = HUGE_VAL;
			tentpos[i].z = HUGE_VAL;
		} else if (lua_istable(l, -1))
			tentpos[i] = get_fvec3(l, -2);
		else
			LERR(l, "tentloc should have 2 nil or table in it");
		lua_pop(l, 1);
	}

	f.send_state_ctf(pid, from, teamname, teamcolor, fog, teamscore, maxscore, holders, intelpos, tentpos, st);
	return 0;
}

static void csend_state_ctf(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st) {
	size_t i;
	(void)st;

	lua_getglobal(l, "send_state_ctf");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, from);

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_pushstring(l, teamname[i]);
		lua_settable(l, -3);
	}

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		push_color(teamcolor[i]);
		lua_settable(l, -3);
	}

	push_color(fog);

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		lua_pushnumber(l, teamscore[i]);
		lua_settable(l, -3);
	}

	lua_pushnumber(l, maxscore);

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		if (holders[i] != -1)
			lua_pushnumber(l, holders[i]);
		else if (intelpos[i].x == HUGE_VAL && intelpos[i].y == HUGE_VAL && intelpos[i].z == HUGE_VAL)
			lua_pushnil(l);
		else
			push_fvec3(intelpos[i]);
		lua_settable(l, -3);
	}

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		if (tentpos[i].x == HUGE_VAL && tentpos[i].y == HUGE_VAL && tentpos[i].z == HUGE_VAL)
			lua_pushnil(l);
		else
			push_fvec3(tentpos[i]);
		lua_settable(l, -3);
	}

	if (lua_pcall(l, 9, 0, 0) != 0)
		CBAIL("send_state_ctf: %s", luaL_checkstring(l, -1));
}

static int lsend_packet(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	size_t length;
	const char *data = luaL_checklstring(l, 2, &length);

	lua_pushnumber(l, f.send_packet(pid, data, length, st));
	return 1;
}

static int csend_packet(plid pid, const void *data, size_t length, struct State *st) {
	int ret;
	(void)st;

	lua_getglobal(l, "send_packet");

	lua_pushnumber(l, pid);
	lua_pushlstring(l, data, length);

	/* TODO: throwing an error with no arg leads to panic due to this checkstring */
	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("send_packet: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("send_packet: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);

	return ret;
}

static int lsend_packet_unreliable(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	size_t length;
	const char *data = luaL_checklstring(l, 2, &length);

	lua_pushnumber(l, f.send_packet_unreliable(pid, data, length, st));
	return 1;
}

static int csend_packet_unreliable(plid pid, const void *data, size_t length, struct State *st) {
	int ret;
	(void)st;

	lua_getglobal(l, "send_packet_unreliable");

	lua_pushnumber(l, pid);
	lua_pushlstring(l, data, length);

	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("send_packet_unreliable: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("send_packet_unreliable: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);

	return ret;
}

static int lon_player_spawn(lua_State *l) {
	plid pid = check_plid(l, 1);
	push_fvec3(f.on_player_spawn(pid, st));
	return 1;
}

static fvec3 con_player_spawn(plid pid, struct State *st) {
	fvec3 ret;

	lua_getglobal(l, "on_player_spawn");

	lua_pushnumber(l, pid);

	/* TODO: think some more about deferring to core implementation with lua errors */
	if (lua_pcall(l, 1, 1, 0) != 0) {
		LOG("on_player_spawn: %s", luaL_checkstring(l, -1));
		return f.on_player_spawn(pid, st);
	}

	/* TODO: make -2 less unexpected -- it really refers to -1 (i think? hope? or can i just use an absolute index?) */
	ret = get_fvec3(l, -2);
	lua_pop(l, 1);

	return ret;
}

/* NOTE: Try not to touch pid_matches' conditions too much while iterating */
int pid_matches(plid broadcast, plid pid, struct State *st);
static int do_piditer(lua_State *l) {
	bplid broadcast = lua_tonumber(l, lua_upvalueindex(1));
	bplid pid = lua_tonumber(l, lua_upvalueindex(2));

	for (;pid<MAX_PLAYERS;pid++) {
		if (pid_matches(broadcast, pid, st)) {
			lua_pushnumber(l, pid);
			lua_pushnumber(l, pid+1);
			lua_replace(l, lua_upvalueindex(2));
			return 1;
		}
	}

	return 0;
}

static int piditer(lua_State *l) {
	/* TODO: is there a better way to both verify that the first arg is a number and to ensure that the first arg is the last? */
	lua_pushnumber(l, luaL_checknumber(l, 1));
	lua_pushnumber(l, 0);
	lua_pushcclosure(l, do_piditer, 2);
	return 1;
}

static int simulate_grenade_physics(lua_State *l) {
	struct Grenade grenade;
	float delta;
	int collided;

	grenade.pos = get_fvec3(l, 1);
	grenade.vel = get_fvec3(l, 2);
	delta = luaL_checknumber(l, 3);

	collided = move_grenade(&grenade, delta, st->globals.map.solidData, 1);

	push_fvec3(grenade.pos);
	push_fvec3(grenade.vel);
	lua_pushboolean(l, collided);

	return 3;
}

clk get_time(void);
double to_s_double(clk ts);

/* TODO: validate index here and elsewhere */
static int get_grenade_detonate_time(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	lua_pushnumber(l, to_s_double(st->globals.grenades[index].detonateTime));
	return 1;
}

static int get_grenade_position(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	push_fvec3(st->globals.grenades[index].pos);
	return 1;
}

static int get_grenade_velocity(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	push_fvec3(st->globals.grenades[index].vel);
	return 1;
}

static int get_grenade_pid(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	lua_pushnumber(l, st->globals.grenades[index].pid);
	return 1;
}

static int get_grenade_team(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	lua_pushnumber(l, st->globals.grenades[index].team);
	return 1;
}

static int set_max_score(lua_State *l) {
	unsigned maxscore = luaL_checknumber(l, 1);
	st->globals.maxscore = maxscore;
	return 0;
}

static int set_team_color(lua_State *l) {
	unsigned team = check_gteamid(l, 1);
	color clr;

	get_color2(l, 2, clr);
	st->globals.teamcolor[team][0] = clr[0];
	st->globals.teamcolor[team][1] = clr[1];
	st->globals.teamcolor[team][2] = clr[2];
	return 0;
}

static int set_team_name(lua_State *l) {
	unsigned team = check_gteamid(l, 1);
	const char *name = luaL_checkstring(l, 2);

	memset(st->globals.teamname[team], 0, 10);
	strncpy(st->globals.teamname[team], name, 9);
	return 0;
}

static int input_on_stdin(lua_State *l) {
	struct pollfd fd[1];
	fd->fd = 0;
	fd->events = POLLIN;

	lua_pushboolean(l, poll(fd, 1, 0) > 0);
	return 1;
}

static int raycast(lua_State *l) {
	fvec3 start = get_fvec3(l, 1);
	fvec3 end = get_fvec3(l, 1);
	int last = lua_toboolean(l, 3);

	ivec3 hitpos;
	int32_t x, y, z;
	int hit;

	hit = cast2(st->globals.map.solidData, start.x, start.y, start.z, end.x, end.y, end.z, 0, &x, &y, &z, last);
	if (!hit)
		return 0;

	/* TODO: align ivec3 or make push_ivec3_3 */
	hitpos.x = x;
	hitpos.y = y;
	hitpos.z = z;
	push_ivec3(hitpos);
	return 1;
}

static int disconnect(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned reason = luaL_checknumber(l, 2);
	enet_peer_disconnect(st->host->peers+pid, reason);
	return 0;
}

static int disconnect_now(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned reason = luaL_checknumber(l, 2);
	enet_peer_disconnect_now(st->host->peers+pid, reason);
	return 0;
}

static int masterlist_set_port(lua_State *l) {
	uint16_t port = luaL_checknumber(l, 1);
	st->ms.port = port;
	return 0;
}

static int masterlist_get_port(lua_State *l) {
	lua_pushnumber(l, st->ms.port);
	return 1;
}

static int masterlist_set_players(lua_State *l) {
	uint8_t players = luaL_checknumber(l, 1);
	st->ms.players = players;
	return 0;
}

static int masterlist_get_players(lua_State *l) {
	lua_pushnumber(l, st->ms.players);
	return 1;
}

static int masterlist_set_max_players(lua_State *l) {
	uint8_t maxplayers = luaL_checknumber(l, 1);
	st->ms.maxplayers = maxplayers;
	return 0;
}

static int masterlist_get_max_players(lua_State *l) {
	lua_pushnumber(l, st->ms.maxplayers);
	return 1;
}

static int masterlist_set_name(lua_State *l) {
	size_t len;
	const char *name = luaL_checklstring(l, 1, &len);
	if (len+1 > sizeof(st->ms.name))
		LERR(l, "masterlist_set_name: name length should be less than %lu", sizeof(st->ms.name));
	memcpy(st->ms.name, name, len+1);
	return 0;
}

static int masterlist_get_name(lua_State *l) {
	lua_pushstring(l, st->ms.name);
	return 1;
}

static int masterlist_set_gamemode(lua_State *l) {
	size_t len;
	const char *gamemode = luaL_checklstring(l, 1, &len);
	if (len+1 > sizeof(st->ms.gamemode))
		LERR(l, "masterlist_set_gamemode: gamemode length should be less than %lu", sizeof(st->ms.gamemode));
	memcpy(st->ms.gamemode, gamemode, len+1);
	return 0;
}

static int masterlist_get_gamemode(lua_State *l) {
	lua_pushstring(l, st->ms.gamemode);
	return 1;
}

static int masterlist_set_map(lua_State *l) {
	size_t len;
	const char *map = luaL_checklstring(l, 1, &len);
	if (len+1 > sizeof(st->ms.map))
		LERR(l, "masterlist_set_map: map length should be less than %lu", sizeof(st->ms.map));
	memcpy(st->ms.map, map, len+1);
	return 0;
}

static int masterlist_get_map(lua_State *l) {
	lua_pushstring(l, st->ms.map);
	return 1;
}

static int lmasterlist_connect(lua_State *l) {
	uint32_t peer;
	ENetAddress addr;
	addr.port = 32886;

	if (enet_address_set_host(&addr, luaL_checkstring(l, 1)) < 0)
		LERR(l, "masterlist_connect: enet_address_set_host: %s", strerror(errno));

	if ((peer = masterlist_connect(&addr, &st->ms)) == (uint32_t)-1)
		LERR(l, "masterlist_connect: enet_host_connect: %s", strerror(errno));

	lua_pushnumber(l, peer);
	return 1;
}

int get_solid(ivec3 pos, struct State *st);
static int is_solid(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1, 0);
	if (pos.z < 0)
		lua_pushboolean(l, 0);
	else
		lua_pushboolean(l, get_solid(pos, st));
	return 1;
}

static int get_fog(lua_State *l) {
	(void)l;
	push_color(st->globals.fog);
	return 1;
}

static int get_team_name(lua_State *l) {
	unsigned team = check_teamid(l, 1);
	/* TODO: support -1 too? */
	if (team == 255)
		lua_pushliteral(l, "Spectator");
	else
		lua_pushstring(l, st->globals.teamname[team]);
	return 1;
}

/* TODO: ammunition estimation */
#if 0
static int get_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, st->p[pid].magAmmo);
	lua_pushnumber(l, st->p[pid].reserveAmmo);
	return 1;
}
#endif

static int get_hp(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, st->p[pid].hp);
	return 1;
}

/* In host byte order */
static int get_ipaddr(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, ntohl(st->host->peers[pid].address.host));
	return 1;
}

/* More popularly referred to as "ping" */
static int get_round_trip_time(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, st->host->peers[pid].roundTripTime);
	return 1;
}

/* TODO: optional team arg? */
static int get_tentloc(lua_State *l) {
	size_t i;

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		if (st->globals.tentpos[i].x == HUGE_VAL && st->globals.tentpos[i].y == HUGE_VAL && st->globals.tentpos[i].z == HUGE_VAL)
			lua_pushnil(l);
		else
			push_fvec3(st->globals.tentpos[i]);
		lua_settable(l, -3);
	}

	return 1;
}

static int get_intelloc(lua_State *l) {
	size_t i;

	lua_newtable(l);
	for (i=0;i<2;i++) {
		lua_pushnumber(l, i+1);
		/* TODO: intelplayers -> intelholders */
		if (st->globals.intelplayers[i] != -1)
			lua_pushnumber(l, st->globals.intelplayers[i]);
		else if (st->globals.intelpos[i].x == HUGE_VAL && st->globals.intelpos[i].y == HUGE_VAL && st->globals.intelpos[i].z == HUGE_VAL)
			lua_pushnil(l);
		else
			push_fvec3(st->globals.intelpos[i]);
		lua_settable(l, -3);
	}

	return 1;
}

static int get_position(lua_State *l) {
	plid pid = check_plid(l, 1);
	push_fvec3(st->p[pid].pos);
	return 1;
}

static int get_orientation(lua_State *l) {
	plid pid = check_plid(l, 1);
	push_fvec3(st->p[pid].ori);
	return 1;
}

static int get_mouse_inputs(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].mouseInputs);
	return 1;
}

/* TODO: weapon -> gun? */
static int get_weapon(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].weapon);
	return 1;
}

/* Weapon that the player will have on next respawn; switching weapon sets this, for example. */
static int get_next_weapon(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].newweapon);
	return 1;
}

static int get_tool(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].tool);
	return 1;
}

static int get_inputs(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].inputs);
	return 1;
}

static int is_airborne(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].airborne);
	return 1;
}

static int is_alive(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].alive);
	return 1;
}

static int is_joined(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].joined);
	return 1;
}

static int is_connected(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->host->peers[pid].state == ENET_PEER_STATE_CONNECTED);
	return 1;
}

static int get_name(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushstring(l, st->p[pid].name);
	return 1;
}

static int get_team(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].team);
	return 1;
}

static int get_team_color(lua_State *l) {
	unsigned team = check_teamid(l, 1);
	color color = {255, 255, 255};

	switch (team) {
	case 0:
		color[0] = st->globals.teamcolor[0][0];
		color[1] = st->globals.teamcolor[0][1];
		color[2] = st->globals.teamcolor[0][2];
		break;
	case 1:
		color[0] = st->globals.teamcolor[1][0];
		color[1] = st->globals.teamcolor[1][1];
		color[2] = st->globals.teamcolor[1][2];
		break;
	}

	push_color(color);
	return 1;
}

static int get_team_score(lua_State *l) {
	unsigned team = check_gteamid(l, 1);
	lua_pushnumber(l, st->globals.teamscore[team]);
	return 1;
}

static int lget_time(struct lua_State *l) {
	lua_pushnumber(l, to_s_double(get_time()));
	return 1;
}

static int lPID_BROADCAST_EXCEPT(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_EXCEPT(luaL_checknumber(l, 1)));
	return 1;
}

static int lPID_BROADCAST_TEAM(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_TEAM(luaL_checknumber(l, 1)));
	return 1;
}

static int lPID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(luaL_checknumber(l, 1), luaL_checknumber(l, 2)));
	return 1;
}

static int lPID_BROADCAST_EXCEPT_TEAM(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_EXCEPT_TEAM(luaL_checknumber(l, 1)));
	return 1;
}

static const struct luaL_Reg funcs[] = {
	/* Add all the cruft from luaawk.h */
	LUA_CALLS
	{"send_state_ctf", lsend_state_ctf},
	{"send_packet", lsend_packet},
	{"send_packet_unreliable", lsend_packet_unreliable},
	{"on_player_spawn", lon_player_spawn},

	/* TODO: these two are not like the rest */
	{"disconnect", disconnect},
	{"disconnect_now", disconnect_now},

	{"masterlist_get_port", masterlist_get_port},
	{"masterlist_get_players", masterlist_get_players},
	{"masterlist_get_max_players", masterlist_get_max_players},
	{"masterlist_get_name", masterlist_get_name},
	{"masterlist_get_map", masterlist_get_map},
	{"masterlist_get_gamemode", masterlist_get_gamemode},

	{"masterlist_set_port", masterlist_set_port},
	{"masterlist_set_players", masterlist_set_players},
	{"masterlist_set_max_players", masterlist_set_max_players},
	{"masterlist_set_name", masterlist_set_name},
	{"masterlist_set_map", masterlist_set_map},
	{"masterlist_set_gamemode", masterlist_set_gamemode},

	{"masterlist_connect", lmasterlist_connect},

	{"piditer", piditer},

	/* Nonportable state-altering funcs -- try to use these when a map
	 * is in the middle of loading if you want defined behavior
	 */
	{"set_max_score", set_max_score},
	{"set_team_name", set_team_name},
	{"set_team_color", set_team_color},

	{"input_on_stdin", input_on_stdin},
	{"raycast", raycast},
	{"simulate_grenade_physics", simulate_grenade_physics},
	{"get_grenade_detonate_time", get_grenade_detonate_time},
	{"get_grenade_position", get_grenade_position},
	{"get_grenade_velocity", get_grenade_velocity},
	{"get_grenade_pid", get_grenade_pid},
	{"get_grenade_team", get_grenade_team},
	{"is_solid", is_solid},
	{"get_fog", get_fog},
	{"get_team_name", get_team_name},
	{"get_hp", get_hp},
	{"get_ipaddr", get_ipaddr},
	{"get_round_trip_time", get_round_trip_time},
	{"get_tentloc", get_tentloc},
	{"get_intelloc", get_intelloc},
	{"get_position", get_position},
	{"get_orientation", get_orientation},
	{"get_mouse_inputs", get_mouse_inputs},
	{"get_weapon", get_weapon},
	{"get_next_weapon", get_next_weapon},
	{"get_tool", get_tool},
	{"get_inputs", get_inputs},
	{"is_airborne", is_airborne},
	{"is_alive", is_alive},
	{"is_joined", is_joined},
	{"is_connected", is_connected},
	{"get_name", get_name},
	{"get_team", get_team},
	{"get_team_color", get_team_color},
	{"get_team_score", get_team_score},
	{"get_time", lget_time},
	{"PID_BROADCAST_EXCEPT", lPID_BROADCAST_EXCEPT},
	{"PID_BROADCAST_TEAM", lPID_BROADCAST_TEAM},
	{"PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER", lPID_BROADCAST_EXCEPT_TEAM_AND_PLAYER},
	{"PID_BROADCAST_EXCEPT_TEAM", lPID_BROADCAST_EXCEPT_TEAM},
	{NULL, NULL}
};

void register_functions(lua_State *l, struct State *st) {
	const struct luaL_Reg *func = funcs;

	while (func->name != NULL) {
		lua_pushcfunction(l, func->func);
		lua_setglobal(l, func->name);

		func++;
	}

	luaL_openlib(l, "server", funcs, 0);

	f = st->f;
	st->f.send_state_ctf = csend_state_ctf;
	st->f.send_packet = csend_packet;
	st->f.send_packet_unreliable = csend_packet_unreliable;
	st->f.on_player_spawn = con_player_spawn;
	register_luaawk(l, st);
}

void hook_lua(const char *cfg, struct State *st2) {
	l = lua_open();

	st = st2;

	luaL_openlibs(l);
	register_functions(l, st);

	lua_pushnumber(l, MAX_PLAYERS);
	lua_setglobal(l, "MAX_PLAYERS");

	lua_pushnumber(l, PID_BROADCAST);
	lua_setglobal(l, "PID_BROADCAST");

	lua_pushnumber(l, PID_COLOR_ANONYMOUS);
	lua_setglobal(l, "PID_COLOR_ANONYMOUS");

	/* TODO: #define SPECTATOR 255? */
	lua_pushnumber(l, 255);
	lua_setglobal(l, "SPECTATOR");

	if (luaL_loadfile(l, "scripts/core.lua") || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load scripts/core.lua: %s", lua_tostring(l, -1));

	if (luaL_loadfile(l, cfg) || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load %s: %s", cfg, lua_tostring(l, -1));

	read_config_values(l, st);
}
