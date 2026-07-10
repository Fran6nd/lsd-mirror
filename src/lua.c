#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "state.h"
#include "demoncore.h"
#include "budgetvxl.h"
#include <poll.h>
#include <setjmp.h>

clk get_time(void);
double to_s_double(clk ts);
clk from_s_double(double ts);

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define LOG1(x) do {st->f.before_log(st); fputs(x"\n", stderr); st->f.after_log(st);}while (0)
#define LERR luaL_error
#define CBAIL(x, ...) do {LOG(x, __VA_ARGS__); return;} while (0)
#define CBAILN1(x, ...) do {LOG(x, __VA_ARGS__); return -1;} while (0)
#define CBAIL1N1(x, ...) do {LOG1(x); return -1;} while (0)

static lua_State *l;

#define STR2(x) #x
#define STR(x) STR2(x)

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

/* TODO: replace set_color and block_action with dedicated bulk build/destroy api? */
plid check_plid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val < 0 || val >= MAX_PLAYERS)
		LERR(l, "plid is invalid (%.0f)", val);
	return val;
}

bplid check_bplid(lua_State *l, int numArg) {
	return luaL_checknumber(l, numArg);
}

nplid check_nplid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val < 0 || val >= 256)
		LERR(l, "nplid is invalid (%.0f)", val);
	return val;
}

unsigned check_teamid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val != 1 && val != 2 && val != 256)
		LERR(l, "teamid is invalid (%.0f)", val);
	return val - 1;
}

unsigned check_gteamid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val != 1 && val != 2)
		LERR(l, "gteamid is invalid (%.0f)", val);
	return val - 1;
}

unsigned check_nteamid(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);
	if (val < 1 || val > 256)
		LERR(l, "nteamid is invalid (%.0f)", val);
	return val - 1;
}

static void push_clk(lua_State *l, clk val) {
	lua_pushnumber(l, to_s_double(val));
}

clk check_clk(lua_State *l, int numArg) {
	double val = luaL_checknumber(l, numArg);

	/* Silently return 0 since clk is unsigned. . . should I make it signed? */
	if (val < 0)
		return 0;

	return from_s_double(val);
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

#define PACKET_OP_FUNC_RETURN(name, check_func) \
	static int l##name(lua_State *l) { \
		plid pid = check_func(l, 1); \
		size_t length; \
		const char *data = luaL_checklstring(l, 2, &length); \
\
		lua_pushnumber(l, f.name(pid, data, length, st)); \
		return 1; \
	} \
\
	static int c##name(plid pid, const void *data, size_t length, struct State *st) { \
		int ret; \
		(void)st; \
\
		lua_getglobal(l, STR(name)); \
\
		lua_pushnumber(l, pid); \
		lua_pushlstring(l, data, length); \
\
		/* TODO: throwing an error with no arg leads to panic due to this checkstring */ \
		if (lua_pcall(l, 2, 1, 0) != 0) \
			CBAILN1(STR(name)": %s", luaL_checkstring(l, -1)); \
\
		if (!lua_isnumber(l, -1)) \
			CBAIL1N1(STR(name)": should return a number"); \
\
		ret = lua_tonumber(l, -1); \
		lua_pop(l, 1); \
\
		return ret; \
	}

#define PACKET_OP_FUNC_NORETURN(name, check_func) \
	static int l##name(lua_State *l) { \
		plid pid = check_func(l, 1); \
		size_t length; \
		const char *data = luaL_checklstring(l, 2, &length); \
\
		f.name(pid, data, length, st); \
		return 0; \
	} \
\
	static void c##name(plid pid, const void *data, size_t length, struct State *st) { \
		(void)st; \
\
		lua_getglobal(l, STR(name)); \
\
		lua_pushnumber(l, pid); \
		lua_pushlstring(l, data, length); \
\
		if (lua_pcall(l, 2, 0, 0) != 0) \
			CBAIL(STR(name)": %s", luaL_checkstring(l, -1)); \
	}

PACKET_OP_FUNC_RETURN(send_packet, check_bplid)
PACKET_OP_FUNC_RETURN(send_packet_unreliable, check_bplid)
PACKET_OP_FUNC_RETURN(on_any_packet, check_plid)
PACKET_OP_FUNC_NORETURN(on_sane_packet, check_plid)
PACKET_OP_FUNC_NORETURN(on_crap_packet, check_plid)

/* TODO: why isn't this done by luaawk? */
static int lget_spawn_position(lua_State *l) {
	plid pid = check_plid(l, 1);
	push_fvec3(f.get_spawn_position(pid, st));
	return 1;
}

static fvec3 cget_spawn_position(plid pid, struct State *st) {
	fvec3 ret;

	lua_getglobal(l, "get_spawn_position");

	lua_pushnumber(l, pid);

	/* TODO: think some more about deferring to core implementation with lua errors */
	if (lua_pcall(l, 1, 1, 0) != 0) {
		LOG("get_spawn_position: %s", luaL_checkstring(l, -1));
		return f.get_spawn_position(pid, st);
	}

	/* TODO: make -2 less unexpected -- it really refers to -1 (i think? hope? or can i just use an absolute index?) */
	ret = get_fvec3(l, -2);
	lua_pop(l, 1);

	return ret;
}

static int lon_version(lua_State *l) {
	size_t msglen;
	plid pid = check_plid(l, 1);
	unsigned idChar = luaL_checknumber(l, 2);
	unsigned major = luaL_checknumber(l, 3);
	unsigned minor = luaL_checknumber(l, 4);
	unsigned patch = luaL_checknumber(l, 5);
	const char *msg = luaL_checklstring(l, 6, &msglen);

	f.on_version(pid, idChar, major, minor, patch, msg, msglen, st);
	return 0;
}

static void con_version(plid pid, unsigned idChar, unsigned major, unsigned minor, unsigned patch, const char *msg, size_t msglen, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_version");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, idChar);
	lua_pushnumber(l, major);
	lua_pushnumber(l, minor);
	lua_pushnumber(l, patch);
	lua_pushlstring(l, msg, msglen);

	if (lua_pcall(l, 6, 0, 0) != 0)
		CBAIL("on_version: %s", luaL_checkstring(l, -1));
}

static int lon_version_ext(lua_State *l) {
	size_t clilen, langlen;
	plid pid = check_plid(l, 1);
	unsigned major = luaL_checknumber(l, 2);
	unsigned minor = luaL_checknumber(l, 3);
	unsigned patch = luaL_checknumber(l, 4);
	uint32_t flags = luaL_checknumber(l, 5);
	const char *cli = luaL_checklstring(l, 6, &clilen);
	const char *lang = luaL_checklstring(l, 7, &langlen);

	f.on_version_ext(pid, major, minor, patch, flags, cli, clilen, lang, langlen, st);
	return 0;
}

static void con_version_ext(plid pid, unsigned major, unsigned minor, unsigned patch, uint32_t flags, const char *cli, size_t clilen, const char *lang, size_t langlen, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_version_ext");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, major);
	lua_pushnumber(l, minor);
	lua_pushnumber(l, patch);
	lua_pushnumber(l, flags);
	lua_pushlstring(l, cli, clilen);
	lua_pushlstring(l, lang, langlen);

	if (lua_pcall(l, 7, 0, 0) != 0)
		CBAIL("on_version_ext: %s", luaL_checkstring(l, -1));
}

static int lload_vxl_from_mem(lua_State *l) {
	size_t len;
	const void *data = luaL_checklstring(l, 1, &len);

	lua_pushnumber(l, f.load_vxl_from_mem(data, len, st));
	return 1;
}

static int cload_vxl_from_mem(const void *data, size_t len, struct State *st) {
	int ret;

	(void)st;
	lua_getglobal(l, "load_vxl_from_mem");

	lua_pushlstring(l, data, len);

	if (lua_pcall(l, 1, 1, 0) != 0)
		CBAILN1("load_vxl_from_mem: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("load_vxl_from_mem: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}

/* NOTE: Try not to touch pid_matches' conditions too much while iterating */
int pid_matches(plid broadcast, plid pid, struct State *st);
static int do_piditer(lua_State *l) {
	bplid broadcast = lua_tonumber(l, lua_upvalueindex(1));
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

static int do_teamiter_all(lua_State *l) {
	const teamid next[3] = {0,2,256};
	teamid team = lua_tonumber(l, lua_upvalueindex(1));

	if (team == 0)
		return 0;

	lua_pushnumber(l, team);
	lua_pushnumber(l, next[team & 0xff]);
	lua_replace(l, lua_upvalueindex(1));
	return 1;
}

static int teamiter_all(lua_State *l) {
	lua_pushnumber(l, 1);
	lua_pushcclosure(l, do_teamiter_all, 1);
	return 1;
}

static int do_teamiter_ingame(lua_State *l) {
	teamid team = lua_tonumber(l, lua_upvalueindex(1));

	if (team == 3)
		return 0;

	lua_pushnumber(l, team);
	lua_pushnumber(l, team+1);
	lua_replace(l, lua_upvalueindex(1));
	return 1;
}

static int teamiter_ingame(lua_State *l) {
	lua_pushnumber(l, 1);
	lua_pushcclosure(l, do_teamiter_ingame, 1);
	return 1;
}

static int do_dump_vxl(lua_State *l) {
	size_t off = lua_tointeger(l, lua_upvalueindex(1));
	if (off >= 512*512)
		return 0;

	{
		uint8_t buf[512*8*65*4];
		size_t buflen = pvx_dump_vxl(&st->globals.map, off % 512, off / 512, 512, 512, 64, buf, 512*8);

		lua_pushlstring(l, (const char *)buf, buflen);

		lua_pushinteger(l, off+512*8);
		lua_replace(l, lua_upvalueindex(1));
		return 1;
	}
}

static int dump_vxl(lua_State *l) {
	lua_pushinteger(l, 0);
	lua_pushcclosure(l, do_dump_vxl, 1);
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

	lua_pushnumber(l, st->globals.grenades[index].team+1);
	return 1;
}

static int get_max_score(lua_State *l) {
	lua_pushnumber(l, st->globals.maxscore);
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

static int set_cull_personality(lua_State *l) {
	int personality = luaL_checknumber(l, 1);
	if (personality < 0 || personality > 1)
		LERR(l, "personality should be one of the CULL_PERSONALITY macros");

	st->globals.cullPersonality = personality;
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
	fvec3 end = get_fvec3(l, 2);
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

#define MAX(x,y) ((x)>(y) ? (x) : (y))
typedef struct {uint32_t x, y, z;} ivec3u;
static int do_iter_block_line(lua_State *l) {
	lua_Integer len = lua_tointeger(l, lua_upvalueindex(13));
	ivec3 cur, step;
	ivec3u d, di;

	if (len-- == 0)
		return 0;

	lua_pushinteger(l, len);
	lua_replace(l, lua_upvalueindex(13));

	cur.x = lua_tointeger(l, lua_upvalueindex(1));
	cur.y = lua_tointeger(l, lua_upvalueindex(2));
	cur.z = lua_tointeger(l, lua_upvalueindex(3));
	push_ivec3(cur);

	d.x = lua_tointeger(l, lua_upvalueindex(4));
	d.y = lua_tointeger(l, lua_upvalueindex(5));
	d.z = lua_tointeger(l, lua_upvalueindex(6));

	di.x = lua_tointeger(l, lua_upvalueindex(7));
	di.y = lua_tointeger(l, lua_upvalueindex(8));
	di.z = lua_tointeger(l, lua_upvalueindex(9));

	step.x = lua_tointeger(l, lua_upvalueindex(10));
	step.y = lua_tointeger(l, lua_upvalueindex(11));
	step.z = lua_tointeger(l, lua_upvalueindex(12));

	#define BITER_CUR(x) lua_upvalueindex(x)
	#define BITER_D(x) lua_upvalueindex(3+(x))
	if (d.z <= d.x && d.z <= d.y) {
		lua_pushinteger(l, cur.z + step.z);
		lua_pushinteger(l, d.z + di.z);
		lua_replace(l, BITER_D(3));
		lua_replace(l, BITER_CUR(3));
	} else if (d.x < d.y) {
		lua_pushinteger(l, cur.x + step.x);
		lua_pushinteger(l, d.x + di.x);
		lua_replace(l, BITER_D(1));
		lua_replace(l, BITER_CUR(1));
	} else {
		lua_pushinteger(l, cur.y + step.y);
		lua_pushinteger(l, d.y + di.y);
		lua_replace(l, BITER_D(2));
		lua_replace(l, BITER_CUR(2));
	}

	return 1;
}

static int iter_block_line(lua_State *l) {
	ivec3 start = get_ivec3(l, 1, 0);
	ivec3 end = get_ivec3(l, 2, 0);
	ivec3 step;
	ivec3u off, d, di;
	uint32_t maxoff;

	step.x = end.x < start.x ? -1 : 1;
	step.y = end.y < start.y ? -1 : 1;
	step.z = end.z < start.z ? -1 : 1;

	off.x = abs(end.x - start.x);
	off.y = abs(end.y - start.y);
	off.z = abs(end.z - start.z);
	maxoff = MAX(MAX(off.x, off.y), off.z);

	di.x = off.x == 0 ? (uint32_t)-1 : maxoff * 1024 / off.x;
	di.y = off.y == 0 ? (uint32_t)-1 : maxoff * 1024 / off.y;
	di.z = off.z == 0 ? (uint32_t)-1 : maxoff * 1024 / off.z;

	d.x = di.x / 2;
	d.y = di.y / 2;
	d.z = di.z / 2;

	if (step.x >= 0)
		d.x = di.x - d.x;
	if (step.y >= 0)
		d.y = di.y - d.y;
	if (step.z >= 0)
		d.z = di.z - d.z;

	lua_pushinteger(l, start.x);
	lua_pushinteger(l, start.y);
	lua_pushinteger(l, start.z);

	lua_pushinteger(l, d.x);
	lua_pushinteger(l, d.y);
	lua_pushinteger(l, d.z);

	lua_pushinteger(l, di.x);
	lua_pushinteger(l, di.y);
	lua_pushinteger(l, di.z);

	lua_pushinteger(l, step.x);
	lua_pushinteger(l, step.y);
	lua_pushinteger(l, step.z);

	lua_pushinteger(l, 1 + off.x + off.y + off.z);

	lua_pushcclosure(l, do_iter_block_line, 13);
	return 1;
}

static int disconnect(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned reason = luaL_checknumber(l, 2);

	if (reason == 2 && st->f.has_quirk(pid, QUIRK_SCREWED_DISCONNECT_DATA, st))
		reason = 10;

	if (st->p[pid].peer)
		enet_peer_disconnect(st->p[pid].peer, reason);
	return 0;
}

static int disconnect_now(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned reason = luaL_checknumber(l, 2);

	if (reason == 2 && st->f.has_quirk(pid, QUIRK_SCREWED_DISCONNECT_DATA, st))
		reason = 10;

	if (st->p[pid].peer)
		enet_peer_disconnect_now(st->p[pid].peer, reason);
	st->p[pid].peer = NULL;
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
		LERR(l, "masterlist_set_name: name length should be less than %"PRIuSIZET, sizeof(st->ms.name));
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
		LERR(l, "masterlist_set_gamemode: gamemode length should be less than %"PRIuSIZET, sizeof(st->ms.gamemode));
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
		LERR(l, "masterlist_set_map: map length should be less than %"PRIuSIZET, sizeof(st->ms.map));
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
	const char *addrstr = luaL_checkstring(l, 1);

	addr.port = luaL_checknumber(l, 2);

	if (enet_address_set_host(&addr, addrstr) < 0)
		LERR(l, "masterlist_connect: enet_address_set_host: %s", strerror(errno));

	if ((peer = masterlist_connect(&addr, &st->ms)) == (uint32_t)-1)
		LERR(l, "masterlist_connect: enet_host_connect: %s", strerror(errno));

	lua_pushnumber(l, peer);
	return 1;
}

static int lmasterlist_disconnect(lua_State *l) {
	uint32_t peer = luaL_checknumber(l, 1);

	masterlist_disconnect(peer, &st->ms);
	return 0;
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

static int get_hp(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, st->p[pid].hp);
	return 1;
}

static int get_score(lua_State *l) {
	plid pid = check_plid(l, 1);
	lua_pushnumber(l, st->p[pid].score);
	return 1;
}

/* In host byte order */
static int get_ipaddr(lua_State *l) {
	plid pid = check_nplid(l, 1);

	if (st->p[pid].peer)
		lua_pushnumber(l, ENET_NET_TO_HOST_32(st->p[pid].peer->address.host));
	else
		lua_pushnumber(l, 0);

	return 1;
}

static int get_udp_port(lua_State *l) {
	plid pid = check_nplid(l, 1);

	if (st->p[pid].peer)
		lua_pushnumber(l, st->p[pid].peer->address.port);
	else
		lua_pushnumber(l, 0);

	return 1;
}

/* More popularly referred to as "ping" */
static int get_round_trip_time(lua_State *l) {
	plid pid = check_plid(l, 1);

	if (st->p[pid].peer)
		lua_pushnumber(l, st->p[pid].peer->roundTripTime);
	else
		lua_pushnumber(l, 500);

	return 1;
}

/* TODO: optional team arg? */
static int get_tentloc(lua_State *l) {
	size_t i;

	lua_newtable(l);
	for (i=0;i<2;i++) {
		if (st->globals.tentpos[i].x == HUGE_VAL && st->globals.tentpos[i].y == HUGE_VAL && st->globals.tentpos[i].z == HUGE_VAL)
			lua_pushnil(l);
		else
			push_fvec3(st->globals.tentpos[i]);
		lua_rawseti(l, -2, i+1);
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

static int get_velocity(lua_State *l) {
	plid pid = check_plid(l, 1);
	push_fvec3(st->p[pid].vel);
	return 1;
}

static int get_mouse_inputs(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].mouseInputs);
	return 1;
}

static int get_team(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].team+1);
	return 1;
}

static int get_gun(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].gun);
	return 1;
}

static int get_next_team(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].newteam+1);
	return 1;
}

/* Gun that the player will have on next respawn; switching gun sets this, for example. */
static int get_next_gun(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].newgun);
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

static int set_reload_time(lua_State *l) {
	plid pid = check_plid(l, 1);
	st->p[pid].reloadtime = check_clk(l, 2);

	return 0;
}

static int get_reload_time(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, to_s_double(st->p[pid].reloadtime));
	return 1;
}

static int get_estimated_mag_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].estMagAmmo);
	return 1;
}

static int get_max_mag_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].maxMagAmmo);
	return 1;
}

static int get_mag_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].estMagAmmo < st->p[pid].maxMagAmmo ? st->p[pid].estMagAmmo : st->p[pid].maxMagAmmo);
	return 1;
}

static int get_reserve_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].reserveAmmo);
	return 1;
}

static int get_block_color(lua_State *l) {
	plid pid = check_plid(l, 1);

	/* TODO: how to handle spectators and dead guys? */
	push_color(st->p[pid].blockColor);
	return 1;
}

/* TODO: you going to finish libpvx2 yet? */
static int get_map_block_color(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1, 1);

	push_color(st->globals.map.colorData+(CALC_I(pos.x, pos.y)+pos.z)*3);
	return 1;
}

static int is_airborne(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].airborne);
	return 1;
}

static int is_wading(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].wade);
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

	lua_pushboolean(l, st->p[pid].connected);
	return 1;
}

static int get_name(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushstring(l, st->p[pid].name);
	return 1;
}

static int get_client_handshaked(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].handshaked);
	return 1;
}

static int get_client_char(lua_State *l) {
	plid pid = check_plid(l, 1);

	if (st->p[pid].idChar == 0)
		lua_pushnil(l);
	else
		lua_pushnumber(l, st->p[pid].idChar);

	return 1;
}

/* TODO: better to return a table? */
static int get_client_major(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verMajor);
	return 1;
}

static int get_client_minor(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verMinor);
	return 1;
}

static int get_client_patch(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verPatch);
	return 1;
}

static int get_client_msg(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushstring(l, st->p[pid].verMsg);
	return 1;
}

static int get_client_ext_supported(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushboolean(l, st->p[pid].hasverext);
	return 1;
}

static int get_client_ext_major(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verExtMajor);
	return 1;
}

static int get_client_ext_minor(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verExtMinor);
	return 1;
}

static int get_client_ext_patch(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verExtPatch);
	return 1;
}

static int get_client_ext_flags(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, st->p[pid].verExtFlags);
	return 1;
}

static int get_client_ext_name(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushstring(l, st->p[pid].verExtCliName);
	return 1;
}

static int get_client_ext_language(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushstring(l, st->p[pid].verExtLang);
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
	lua_pushnumber(l, PID_BROADCAST_EXCEPT(check_plid(l, 1)));
	return 1;
}

static int lPID_BROADCAST_TEAM(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_TEAM(check_teamid(l, 1)));
	return 1;
}

static int lPID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER(check_teamid(l, 1), check_plid(l, 2)));
	return 1;
}

static int lPID_BROADCAST_EXCEPT_TEAM(struct lua_State *l) {
	lua_pushnumber(l, PID_BROADCAST_EXCEPT_TEAM(check_teamid(l, 1)));
	return 1;
}

static const struct luaL_Reg funcs[] = {
	/* Add all the cruft from luaawk.h */
	LUA_CALLS
	{"send_state_ctf", lsend_state_ctf},
	{"send_packet", lsend_packet},
	{"send_packet_unreliable", lsend_packet_unreliable},
	{"on_any_packet", lon_any_packet},
	{"on_sane_packet", lon_sane_packet},
	{"on_crap_packet", lon_crap_packet},
	{"get_spawn_position", lget_spawn_position},
	{"on_version", lon_version},
	{"on_version_ext", lon_version_ext},
	{"load_vxl_from_mem", lload_vxl_from_mem},

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
	{"masterlist_disconnect", lmasterlist_disconnect},

	{"piditer", piditer},
	{"teamiter_all", teamiter_all},
	{"teamiter_ingame", teamiter_ingame},
	{"dump_vxl", dump_vxl},
	{"iter_block_line", iter_block_line},

	/* Nonportable state-altering funcs -- try to use these when a map
	 * is in the middle of loading if you want defined behavior
	 */
	{"set_max_score", set_max_score},
	{"set_team_name", set_team_name},
	{"set_team_color", set_team_color},

	{"set_cull_personality", set_cull_personality},

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
	{"get_hp", get_hp},
	{"get_score", get_score},
	{"get_ipaddr", get_ipaddr},
	{"get_udp_port", get_udp_port},
	{"get_round_trip_time", get_round_trip_time},
	{"get_tentloc", get_tentloc},
	{"get_intelloc", get_intelloc},
	{"get_position", get_position},
	{"get_orientation", get_orientation},
	{"get_velocity", get_velocity},
	{"get_mouse_inputs", get_mouse_inputs},
	{"get_team", get_team},
	{"get_gun", get_gun},
	{"get_next_team", get_next_team},
	{"get_next_gun", get_next_gun},
	{"get_tool", get_tool},
	{"get_inputs", get_inputs},
	{"set_reload_time", set_reload_time},
	{"get_reload_time", get_reload_time},
	{"get_estimated_mag_ammo", get_estimated_mag_ammo},
	{"get_max_mag_ammo", get_max_mag_ammo},
	{"get_mag_ammo", get_mag_ammo},
	{"get_reserve_ammo", get_reserve_ammo},
	/* TODO: unfortunate naming */
	{"get_block_color", get_block_color},
	{"get_map_block_color", get_map_block_color},
	{"is_airborne", is_airborne},
	{"is_wading", is_wading},
	{"is_alive", is_alive},
	{"is_joined", is_joined},
	{"is_connected", is_connected},
	{"get_name", get_name},
	{"get_client_handshaked", get_client_handshaked},
	{"get_client_char", get_client_char},
	{"get_client_major", get_client_major},
	{"get_client_minor", get_client_minor},
	{"get_client_patch", get_client_patch},
	{"get_client_msg", get_client_msg},
	{"get_client_ext_supported", get_client_ext_supported},
	{"get_client_ext_major", get_client_ext_major},
	{"get_client_ext_minor", get_client_ext_minor},
	{"get_client_ext_patch", get_client_ext_patch},
	{"get_client_ext_flags", get_client_ext_flags},
	{"get_client_ext_name", get_client_ext_name},
	{"get_client_ext_language", get_client_ext_language},
	{"get_team_name", get_team_name},
	{"get_team_color", get_team_color},
	{"get_team_score", get_team_score},
	{"get_max_score", get_max_score},
	{"get_time", lget_time},
	{"PID_BROADCAST_EXCEPT", lPID_BROADCAST_EXCEPT},
	{"PID_BROADCAST_TEAM", lPID_BROADCAST_TEAM},
	{"PID_BROADCAST_EXCEPT_TEAM_AND_PLAYER", lPID_BROADCAST_EXCEPT_TEAM_AND_PLAYER},
	{"PID_BROADCAST_EXCEPT_TEAM", lPID_BROADCAST_EXCEPT_TEAM},
	{NULL, NULL}
};

void register_functions(lua_State *l, struct State *st) {
#if (LUA_VERSION_NUM >= 502)
	lua_pushinteger(l, LUA_RIDX_GLOBALS);
	lua_gettable(l, LUA_REGISTRYINDEX);
	luaL_setfuncs(l, funcs, 0);
#else
	lua_pushvalue(l, LUA_GLOBALSINDEX);
	luaL_register(l, NULL, funcs);
#endif
	lua_pop(l, 1);

#if (LUA_VERSION_NUM >= 502)
	lua_createtable(l, 0, sizeof(funcs)/sizeof(*funcs)-1);
	luaL_setfuncs(l, funcs, 0);
	lua_setglobal(l, "server");
#else
	luaL_register(l, "server", funcs);
	lua_pop(l, 1);
#endif

	f = st->f;
	st->f.send_state_ctf = csend_state_ctf;
	st->f.send_packet = csend_packet;
	st->f.send_packet_unreliable = csend_packet_unreliable;
	st->f.on_any_packet = con_any_packet;
	st->f.on_sane_packet = con_sane_packet;
	st->f.on_crap_packet = con_crap_packet;
	st->f.get_spawn_position = cget_spawn_position;
	st->f.on_version = con_version;
	st->f.on_version_ext = con_version_ext;
	st->f.load_vxl_from_mem = cload_vxl_from_mem;
	register_luaawk(l, st);
}

jmp_buf lua_panicenv;
static lua_CFunction panicf;
static int panic(lua_State *l) {
	panicf(l);
	longjmp(lua_panicenv, 1);
}

void hook_lua(const char *cfg, unsigned long port, struct State *st2) {
	l = luaL_newstate();
	if (l == NULL)
		ERR("luaL_newstate");

	st = st2;

	luaL_openlibs(l);
	register_functions(l, st);

	#define SETMACRO(macro) do {lua_pushnumber(l, macro); lua_setglobal(l, #macro);} while (0)
	SETMACRO(MAX_PLAYERS);
	SETMACRO(PID_BROADCAST);

	/* TODO: #define SPECTATOR 255? */
	lua_pushnumber(l, 256);
	lua_setglobal(l, "SPECTATOR");

	lua_pushnumber(l, port);
	lua_setglobal(l, "ENET_PORT");

	SETMACRO(CULL_PERSONALITY_VOXLAP);
	SETMACRO(CULL_PERSONALITY_OPENSPADES);

	SETMACRO(QUIRK_INFLOOR);
	SETMACRO(QUIRK_NOSHORTPLAYER);
	SETMACRO(QUIRK_SCREWED_DISCONNECT_DATA);
	SETMACRO(QUIRK_OS_CP437);
	SETMACRO(QUIRK_UTF8);
	SETMACRO(QUIRK_ASCII);
	SETMACRO(QUIRK_OS_BACTION_CULL);
	SETMACRO(QUIRK_UTF8_COLOR_IMG);
	SETMACRO(QUIRK_INSKY);

	if (luaL_loadfile(l, "scripts/core.lua") || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load scripts/core.lua: %s", lua_tostring(l, -1));

	if (luaL_loadfile(l, cfg) || lua_pcall(l, 0, 0, 0))
		LERR(l, "Can't load %s: %s", cfg, lua_tostring(l, -1));

	read_config_values(l, st);
}

void setpanic_lua(void) {
	panicf = lua_atpanic(l, panic);
}

void close_lua(void) {
	lua_close(l);
}
