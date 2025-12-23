#include <stdio.h>
#include <math.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lualib.h>
#include "state.h"

#define LOG(x, ...) fprintf(stderr, x"\n", __VA_ARGS__)
#define LOG1(x) fputs(x"\n", stderr);
//#define LERR(l, ...) LOG(__VA_ARGS__)
#define LERR luaL_error
#define CBAIL(x, ...) do {fprintf(stderr, x"\n", __VA_ARGS__); return;} while (0)

static lua_State *l;

/* TODO: color -> struct */
void get_color2(lua_State *l, int table, color color) {
	lua_pushstring(l, "b");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[0] = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "g");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[1] = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "r");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "color should have 3 RGB numbers in it");

	color[2] = lua_tonumber(l, -1);
	lua_pop(l, 1);
}

ivec3 get_ivec3(lua_State *l, int table) {
	ivec3 val;

	lua_pushstring(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.x = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.y = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "z");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "ivec3 should have 3 numbers in it");

	val.z = lua_tonumber(l, -1);
	lua_pop(l, 1);

	return val;
}

fvec3 get_fvec3(lua_State *l, int table) {
	fvec3 val;

	lua_pushstring(l, "x");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3 should have 3 numbers in it");

	val.x = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "y");
	lua_gettable(l, table);
	if (!lua_isnumber(l, -1))
		LERR(l, "fvec3 should have 3 numbers in it");

	val.y = lua_tonumber(l, -1);
	lua_pop(l, 1);


	lua_pushstring(l, "z");
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

const char *get_name(lua_State *l, unsigned team) {
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
	strncpy(st->globals.teamname[0], get_name(l, 1), 9);

	memset(st->globals.teamname[1], 0, 10);
	strncpy(st->globals.teamname[1], get_name(l, 2), 9);
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
static int ltick(lua_State *l) {
	f.tick(st);
	return 0;
}

static void ctick(struct State *st) {
	lua_getglobal(l, "tick");
	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("tick: %s", luaL_checkstring(l, -1));
}

/* TODO: macroize and check inputs */
static int lset_color(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	color color;

	get_color2(l, 2, color);

	f.set_color(pid, color, st);
	return 0;
}

static void cset_color(plid pid, color color, struct State *st) {
	lua_getglobal(l, "set_color");

	lua_pushnumber(l, pid);
	push_color(color);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_color: %s", luaL_checkstring(l, -1));
}

static int lsend_chat(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	const char *msg = luaL_checkstring(l, 2);
	unsigned type = luaL_checknumber(l, 3);
	plid from = luaL_checknumber(l, 4);

	f.send_chat(pid, msg, type, from, st);
	return 0;
}

static void csend_chat(plid pid, const char *msg, unsigned type, plid from, struct State *st) {
	lua_getglobal(l, "send_chat");

	lua_pushnumber(l, pid);
	lua_pushstring(l, msg);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_chat: %s", luaL_checkstring(l, -1));
}

static int lon_color_change(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	color color;

	get_color2(l, 2, color);

	f.on_color_change(pid, color, st);
	return 0;
}

/* TODO: send new color back to pid *only* */
static void con_color_change(plid pid, color color, struct State *st) {
	lua_getglobal(l, "on_color_change");

	lua_pushnumber(l, pid);
	push_color(color);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_color_change: %s", luaL_checkstring(l, -1));
}

static int lspawn_player(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	f.spawn_player(pid, st);
	return 0;
}

/* TODO: send new color back to pid *only* */
static void cspawn_player(plid pid, struct State *st) {
	lua_getglobal(l, "spawn_player");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("spawn_player: %s", luaL_checkstring(l, -1));
}

/* TODO: move name to 2nd arg */
static int lon_join(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	unsigned team = luaL_checknumber(l, 2);
	unsigned weapon = luaL_checknumber(l, 3);
	const char *name = luaL_checkstring(l, 4);

	f.on_join(pid, team, weapon, name, st);
	return 0;
}

/* TODO: send new color back to pid *only* */
static void con_join(plid pid, unsigned team, unsigned weapon, const char *name, struct State *st) {
	lua_getglobal(l, "on_join");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team);
	lua_pushnumber(l, weapon);
	lua_pushstring(l, name);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("on_join: %s", luaL_checkstring(l, -1));
}

static int lblock_action(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1);
	unsigned type = luaL_checknumber(l, 2);
	plid from = luaL_checknumber(l, 3);

	f.block_action(pos, type, from, st);
	return 0;
}

/* TODO: send new color back to pid *only* */
static void cblock_action(ivec3 pos, unsigned type, plid from, struct State *st) {
	lua_getglobal(l, "block_action");

	push_ivec3(pos);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("block_action: %s", luaL_checkstring(l, -1));
}

static int lon_block_action(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	ivec3 pos = get_ivec3(l, 2);
	unsigned type = luaL_checknumber(l, 3);

	f.on_block_action(pid, pos, type, st);
	return 0;
}

/* TODO: send new color back to pid *only* */
static void con_block_action(plid pid, ivec3 pos, unsigned type, struct State *st) {
	lua_getglobal(l, "on_block_action");

	lua_pushnumber(l, pid);
	push_ivec3(pos);
	lua_pushnumber(l, type);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_block_action: %s", luaL_checkstring(l, -1));
}

static int lon_chat(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	const char *msg = luaL_checkstring(l, 2);
	unsigned type = luaL_checknumber(l, 3);

	f.on_chat(pid, msg, type, st);
	return 0;
}

static void con_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	lua_getglobal(l, "on_chat");

	lua_pushnumber(l, pid);
	lua_pushstring(l, msg);
	lua_pushnumber(l, type);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_chat: %s", luaL_checkstring(l, -1));
}

static int lkill(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	unsigned type = luaL_checknumber(l, 2);
	plid killer = luaL_checknumber(l, 3);

	f.kill(pid, type, killer, st);
	return 0;
}

static void ckill(plid pid, unsigned type, plid killer, struct State *st) {
	lua_getglobal(l, "kill");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, type);
	lua_pushnumber(l, killer);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("kill: %s", luaL_checkstring(l, -1));
}

static int lset_jump(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	f.set_jump(pid, st);
	return 0;
}

static void cset_jump(plid pid, struct State *st) {
	lua_getglobal(l, "set_jump");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("set_jump: %s", luaL_checkstring(l, -1));
}

static int ltick_player_physics(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	float timeDelta = luaL_checknumber(l, 2);

	f.tick_player_physics(pid, timeDelta, st);
	return 0;
}

static void ctick_player_physics(plid pid, float timeDelta, struct State *st) {
	lua_getglobal(l, "tick_player_physics");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, timeDelta);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("tick_player_physics: %s", luaL_checkstring(l, -1));
}

static int lset_position(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.set_position(pid, pos, st);
	return 0;
}

static void cset_position(plid pid, fvec3 pos, struct State *st) {
	lua_getglobal(l, "set_position");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_position: %s", luaL_checkstring(l, -1));
}

/* TODO: load_map_from_mem? */
static int lload_map_from_file(lua_State *l) {
	const char *path = luaL_checkstring(l, 1);

	f.load_map_from_file(path, st);
	return 0;
}

static void cload_map_from_file(const char *path, struct State *st) {
	lua_getglobal(l, "load_map_from_file");

	lua_pushstring(l, path);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("load_map_from_file: %s", luaL_checkstring(l, -1));
}

static int lon_position(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.on_position(pid, pos, st);
	return 0;
}

static void con_position(plid pid, fvec3 pos, struct State *st) {
	lua_getglobal(l, "on_position");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_position: %s", luaL_checkstring(l, -1));
}

static int lcapture_intel(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	unsigned winning = lua_toboolean(l, 2);

	f.capture_intel(pid, winning, st);
	return 0;
}

static void ccapture_intel(plid pid, unsigned winning, struct State *st) {
	lua_getglobal(l, "capture_intel");

	lua_pushnumber(l, pid);
	lua_pushboolean(l, winning);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("capture_intel: %s", luaL_checkstring(l, -1));
}

static int lpickup_intel(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	f.pickup_intel(pid, st);
	return 0;
}

static void cpickup_intel(plid pid, struct State *st) {
	lua_getglobal(l, "pickup_intel");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("pickup_intel: %s", luaL_checkstring(l, -1));
}

static int ldrop_intel(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.drop_intel(pid, pos, st);
	return 0;
}

static void cdrop_intel(plid pid, fvec3 pos, struct State *st) {
	lua_getglobal(l, "drop_intel");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("drop_intel: %s", luaL_checkstring(l, -1));
}

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

static int lrestock(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	f.restock(pid, st);
	return 0;
}

static void crestock(plid pid, struct State *st) {
	lua_getglobal(l, "restock");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("restock: %s", luaL_checkstring(l, -1));
}


static int lmove_intel(lua_State *l) {
	unsigned team = luaL_checknumber(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.move_intel(team, pos, st);
	return 0;
}

static void cmove_intel(unsigned team, fvec3 pos, struct State *st) {
	lua_getglobal(l, "move_intel");

	lua_pushnumber(l, team);
	/* TODO: l arg discrepency between get_fvec3, push_fvec3 */
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("move_intel: %s", luaL_checkstring(l, -1));
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

static int is_alive(lua_State *l) {
	plid pid = luaL_checknumber(l, 1);

	lua_pushboolean(l, st->p[pid].alive);
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

clk get_time(void);
double to_s_double(clk ts);

static int lget_time(struct lua_State *l) {
	lua_pushnumber(l, to_s_double(get_time()));
	return 1;
}

static const struct luaL_Reg funcs[] = {
	{"tick", ltick},
	{"set_color", lset_color},
	{"send_chat", lsend_chat},
	{"on_color_change", lon_color_change},
	{"spawn_player", lspawn_player},
	{"on_join", lon_join},
	{"block_action", lblock_action},
	{"on_block_action", lon_block_action},
	{"on_chat", lon_chat},
	{"kill", lkill},
	{"set_jump", lset_jump},
	{"tick_player_physics", ltick_player_physics},
	{"on_position", lon_position},
	{"set_position", lset_position},
	{"load_map_from_file", lload_map_from_file},
	{"capture_intel", lcapture_intel},
	{"pickup_intel", lpickup_intel},
	{"drop_intel", ldrop_intel},
	{"send_state_ctf", lsend_state_ctf},
	{"restock", lrestock},
	{"move_intel", lmove_intel},

	{"get_intelloc", get_intelloc},
	{"get_position", get_position},
	{"get_orientation", get_orientation},
	{"get_inputs", get_inputs},
	{"is_alive", is_alive},
	{"get_team", get_team},
	{"get_team_color", get_team_color},
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
	st->f.tick = ctick;
	st->f.set_color = cset_color;
	st->f.send_chat = csend_chat;
	st->f.on_color_change = con_color_change;
	st->f.spawn_player = cspawn_player;
	st->f.on_join = con_join;
	st->f.on_block_action = con_block_action;
	st->f.block_action = cblock_action;
	st->f.on_chat = con_chat;
	st->f.kill = ckill;
	st->f.set_jump = cset_jump;
	st->f.tick_player_physics = ctick_player_physics;
	st->f.set_position = cset_position;
	st->f.on_position = con_position;
	st->f.load_map_from_file = cload_map_from_file;
	st->f.capture_intel = ccapture_intel;
	st->f.pickup_intel = cpickup_intel;
	st->f.drop_intel = cdrop_intel;
	st->f.send_state_ctf = csend_state_ctf;
	st->f.restock = crestock;
}

/* TODO: lua config file. . ? */
void hook_lua(struct State *st2) {
	l = lua_open();

	st = st2;

	luaL_openlibs(l);
	register_functions(l, st);

	if (luaL_loadfile(l, "config.lua") || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load config.lua: %s", lua_tostring(l, -1));

	read_config_values(l, st);
}
