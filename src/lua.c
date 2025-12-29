#include <stdio.h>
#include <math.h>
#include <arpa/inet.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lualib.h>
#include "state.h"

#define LOG(x, ...) fprintf(stderr, x"\n", __VA_ARGS__)
#define LOG1(x) fputs(x"\n", stderr);
#define LERR luaL_error
#define CBAIL(x, ...) do {fprintf(stderr, x"\n", __VA_ARGS__); return;} while (0)
#define CBAILN1(x, ...) do {fprintf(stderr, x"\n", __VA_ARGS__); return -1;} while (0)
#define CBAIL1N1(x, ...) do {fputs(x"\n", stderr); return -1;} while (0)

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

ivec3 get_ivec3(lua_State *l, int table) {
	ivec3 val;

	lua_pushliteral(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.x = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.y = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "z");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.z = lua_tonumber(l, -1);
	lua_pop(l, 1);

	return val;
}

fvec3 get_fvec3(lua_State *l, int table) {
	fvec3 val;

	lua_pushliteral(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3 should have 3 numbers in it");

	val.x = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3 should have 3 numbers in it");

	val.y = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushliteral(l, "z");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3 should have 3 numbers in it");

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

const char *get_team_name(lua_State *l, unsigned team) {
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
	lua_getglobal(l, "fog");
	if (!lua_istable(l, -1))
		LERR(l, "fog should be a table");

	get_color2(l, -2, st->globals.fog);
	lua_settop(l, 0);


	lua_getglobal(l, "team_name");
	if (!lua_istable(l, -1))
		LERR(l, "team_name should be a table");

	memset(st->globals.teamname[0], 0, 10);
	strncpy(st->globals.teamname[0], get_team_name(l, 1), 9);

	memset(st->globals.teamname[1], 0, 10);
	strncpy(st->globals.teamname[1], get_team_name(l, 2), 9);
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
	plid pid = luaL_checknumber(l, 1);
	plid from = luaL_checknumber(l, 2);
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
		tentpos[i] = get_fvec3(l, -2);
		lua_pop(l, 1);
	}

	f.send_state_ctf(pid, from, teamname, teamcolor, fog, teamscore, maxscore, holders, intelpos, tentpos, st);
	return 0;
}

static void csend_state_ctf(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st) {
	size_t i;

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
		push_fvec3(tentpos[i]);
		lua_settable(l, -3);
	}

	if (lua_pcall(l, 9, 0, 0) != 0)
		CBAIL("send_state_ctf: %s", luaL_checkstring(l, -1));
}

static int lsend_packet(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	size_t length;
	const char *data = luaL_checklstring(l, 2, &length);

	lua_pushnumber(l, f.send_packet(pid, data, length, st));
	return 1;
}

static int csend_packet(plid pid, const void *data, size_t length, struct State *st) {
	int ret;

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
	plid pid = luaL_checknumber(l, 1);
	size_t length;
	const char *data = luaL_checklstring(l, 2, &length);

	lua_pushnumber(l, f.send_packet_unreliable(pid, data, length, st));
	return 1;
}

static int csend_packet_unreliable(plid pid, const void *data, size_t length, struct State *st) {
	int ret;

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

/* NOTE: Try not to touch pid_matches' conditions too much while iterating */
int pid_matches(plid broadcast, plid pid, struct State *st);
static int do_piditer(lua_State *l) {
	plid broadcast = lua_tonumber(l, lua_upvalueindex(1));
	plid pid = lua_tonumber(l, lua_upvalueindex(2));

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

static int disconnect(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	unsigned reason = luaL_checknumber(l, 2);
	enet_peer_disconnect(st->host->peers+pid, reason);
	return 0;
}

static int disconnect_now(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	unsigned reason = luaL_checknumber(l, 2);
	enet_peer_disconnect_now(st->host->peers+pid, reason);
	return 0;
}

static int get_hp(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	lua_pushnumber(l, st->p[pid].hp);
	return 1;
}

/* In host byte order */
static int get_ipaddr(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	lua_pushnumber(l, ntohl(st->host->peers[pid].address.host));
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
	plid pid = luaL_checknumber(l, 1);
	push_fvec3(st->p[pid].pos);
	return 1;
}

static int get_orientation(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	push_fvec3(st->p[pid].ori);
	return 1;
}

static int get_inputs(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushnumber(l, st->p[pid].inputs);
	return 1;
}

static int is_airborne(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushboolean(l, st->p[pid].airborne);
	return 1;
}

static int is_alive(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushboolean(l, st->p[pid].alive);
	return 1;
}

static int is_joined(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushboolean(l, st->p[pid].connected);
	return 1;
}

/* TODO: rename connected -> joined */
static int is_connected(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushboolean(l, st->host->peers[pid].state == ENET_PEER_STATE_CONNECTED);
	return 1;
}

static int get_name(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushstring(l, st->p[pid].name);
	return 1;
}

static int get_team(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushnumber(l, st->p[pid].team);
	return 1;
}

static int get_team_color(lua_State *l) {
	unsigned team = luaL_checknumber(l, 1);
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
	unsigned team = luaL_checknumber(l, 1);
	lua_pushnumber(l, st->globals.teamscore[team]);
	return 1;
}

clk get_time(void);
double to_s_double(clk ts);

static int lget_time(struct lua_State *l) {
	lua_pushnumber(l, to_s_double(get_time()));
	return 1;
}

static const struct luaL_Reg funcs[] = {
	/* Add all the cruft from luaawk.h */
	LUA_CALLS
	{"send_state_ctf", lsend_state_ctf},
	{"send_packet", lsend_packet},
	{"send_packet_unreliable", lsend_packet_unreliable},

	/* TODO: these two are not like the rest */
	{"disconnect", disconnect},
	{"disconnect_now", disconnect_now},

	{"piditer", piditer},

	{"get_hp", get_hp},
	{"get_ipaddr", get_ipaddr},
	{"get_intelloc", get_intelloc},
	{"get_position", get_position},
	{"get_orientation", get_orientation},
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
	register_luaawk(l, st);
}

/* TODO: lua config file. . ? */
void hook_lua(struct State *st2) {
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

	if (luaL_loadfile(l, "config.lua") || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load config.lua: %s", lua_tostring(l, -1));

	read_config_values(l, st);
}
