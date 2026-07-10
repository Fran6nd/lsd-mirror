#ifndef LS2_SERVER_LUAAWK_H
#define LS2_SERVER_LUAAWK_H

static int lassign_new_pid(lua_State *l) {
	(void)l;

	lua_pushnumber(l, f.assign_new_pid(st));
	return 1;
}

static plid cassign_new_pid(struct State *st) {
	plid ret;

	(void)st;
	lua_getglobal(l, "assign_new_pid");


	if (lua_pcall(l, 0, 1, 0) != 0)
		CBAILN1("assign_new_pid: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("assign_new_pid: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lon_any_connect(lua_State *l) {
	nplid pid = check_nplid(l, 1);

	f.on_any_connect(pid, st);
	return 0;
}

static void con_any_connect(nplid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_any_connect");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_any_connect: %s", luaL_checkstring(l, -1));
}


static int lon_successful_connect(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.on_successful_connect(pid, st);
	return 0;
}

static void con_successful_connect(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_successful_connect");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_successful_connect: %s", luaL_checkstring(l, -1));
}


static int lon_disconnect(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.on_disconnect(pid, st);
	return 0;
}

static void con_disconnect(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_disconnect");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_disconnect: %s", luaL_checkstring(l, -1));
}


static int lon_join(lua_State *l) {
	plid pid = check_plid(l, 1);
	teamid team = check_teamid(l, 2);
	unsigned gun = luaL_checknumber(l, 3);
	const char *name = luaL_checkstring(l, 4);

	f.on_join(pid, team, gun, name, st);
	return 0;
}

static void con_join(plid pid, teamid team, unsigned gun, const char *name, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_join");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team+1);
	lua_pushnumber(l, gun);
	lua_pushstring(l, name);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("on_join: %s", luaL_checkstring(l, -1));
}


static int lon_switch(lua_State *l) {
	plid pid = check_plid(l, 1);
	teamid team = check_teamid(l, 2);
	unsigned gun = luaL_checknumber(l, 3);

	f.on_switch(pid, team, gun, st);
	return 0;
}

static void con_switch(plid pid, teamid team, unsigned gun, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_switch");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team+1);
	lua_pushnumber(l, gun);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_switch: %s", luaL_checkstring(l, -1));
}


static int lon_position(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.on_position(pid, pos, st);
	return 0;
}

static void con_position(plid pid, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_position");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_position: %s", luaL_checkstring(l, -1));
}


static int lon_orientation(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 ori = get_fvec3(l, 2);

	f.on_orientation(pid, ori, st);
	return 0;
}

static void con_orientation(plid pid, fvec3 ori, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_orientation");

	lua_pushnumber(l, pid);
	push_fvec3(ori);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_orientation: %s", luaL_checkstring(l, -1));
}


static int lon_move_input(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned bitmask = luaL_checknumber(l, 2);

	f.on_move_input(pid, bitmask, st);
	return 0;
}

static void con_move_input(plid pid, unsigned bitmask, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_move_input");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, bitmask);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_move_input: %s", luaL_checkstring(l, -1));
}


static int lon_mouse_input(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned bitmask = luaL_checknumber(l, 2);

	f.on_mouse_input(pid, bitmask, st);
	return 0;
}

static void con_mouse_input(plid pid, unsigned bitmask, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_mouse_input");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, bitmask);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_mouse_input: %s", luaL_checkstring(l, -1));
}


static int lon_color_change(lua_State *l) {
	plid pid = check_plid(l, 1);
	color color;

	get_color2(l, 2, color);

	f.on_color_change(pid, color, st);
	return 0;
}

static void con_color_change(plid pid, color color, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_color_change");

	lua_pushnumber(l, pid);
	push_color(color);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_color_change: %s", luaL_checkstring(l, -1));
}


static int lon_block_action(lua_State *l) {
	plid pid = check_plid(l, 1);
	ivec3 pos = get_ivec3(l, 2, 1);
	unsigned type = luaL_checknumber(l, 3);

	f.on_block_action(pid, pos, type, st);
	return 0;
}

static void con_block_action(plid pid, ivec3 pos, unsigned type, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_block_action");

	lua_pushnumber(l, pid);
	push_ivec3(pos);
	lua_pushnumber(l, type);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_block_action: %s", luaL_checkstring(l, -1));
}


static int lon_block_line(lua_State *l) {
	plid pid = check_plid(l, 1);
	ivec3 start = get_ivec3(l, 2, 1);
	ivec3 end = get_ivec3(l, 3, 1);

	f.on_block_line(pid, start, end, st);
	return 0;
}

static void con_block_line(plid pid, ivec3 start, ivec3 end, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_block_line");

	lua_pushnumber(l, pid);
	push_ivec3(start);
	push_ivec3(end);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_block_line: %s", luaL_checkstring(l, -1));
}


static int lon_chat(lua_State *l) {
	plid pid = check_plid(l, 1);
	const char *msg = luaL_checkstring(l, 2);
	unsigned type = luaL_checknumber(l, 3);

	f.on_chat(pid, msg, type, st);
	return 0;
}

static void con_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_chat");

	lua_pushnumber(l, pid);
	lua_pushstring(l, msg);
	lua_pushnumber(l, type);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_chat: %s", luaL_checkstring(l, -1));
}


static int lon_tool_change(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned tool = luaL_checknumber(l, 2);

	f.on_tool_change(pid, tool, st);
	return 0;
}

static void con_tool_change(plid pid, unsigned tool, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_tool_change");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, tool);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("on_tool_change: %s", luaL_checkstring(l, -1));
}


static int lon_hit(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned type = luaL_checknumber(l, 2);
	plid hitPlayer = check_plid(l, 3);

	f.on_hit(pid, type, hitPlayer, st);
	return 0;
}

static void con_hit(plid pid, unsigned type, plid hitPlayer, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_hit");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, type);
	lua_pushnumber(l, hitPlayer);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_hit: %s", luaL_checkstring(l, -1));
}


static int lon_grenade(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 pos = get_fvec3(l, 2);
	fvec3 vel = get_fvec3(l, 3);
	float fuse = luaL_checknumber(l, 4);

	f.on_grenade(pid, pos, vel, fuse, st);
	return 0;
}

static void con_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_grenade");

	lua_pushnumber(l, pid);
	push_fvec3(pos);
	push_fvec3(vel);
	lua_pushnumber(l, fuse);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("on_grenade: %s", luaL_checkstring(l, -1));
}


static int lon_reload(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.on_reload(pid, st);
	return 0;
}

static void con_reload(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_reload");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_reload: %s", luaL_checkstring(l, -1));
}


static int lon_quirks(lua_State *l) {
	plid pid = check_plid(l, 1);
	const char *data = luaL_checkstring(l, 2);
	size_t len = luaL_checknumber(l, 3);

	f.on_quirks(pid, data, len, st);
	return 0;
}

static void con_quirks(plid pid, const char *data, size_t len, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_quirks");

	lua_pushnumber(l, pid);
	lua_pushstring(l, data);
	lua_pushnumber(l, len);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("on_quirks: %s", luaL_checkstring(l, -1));
}


static int lon_handshake(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.on_handshake(pid, st);
	return 0;
}

static void con_handshake(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_handshake");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_handshake: %s", luaL_checkstring(l, -1));
}


static int lget_spawn_time(lua_State *l) {
	plid pid = check_plid(l, 1);

	lua_pushnumber(l, f.get_spawn_time(pid, st));
	return 1;
}

static clk cget_spawn_time(plid pid, struct State *st) {
	clk ret;

	(void)st;
	lua_getglobal(l, "get_spawn_time");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 1, 0) != 0)
		CBAILN1("get_spawn_time: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("get_spawn_time: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lget_hit_damage(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned type = luaL_checknumber(l, 2);

	lua_pushnumber(l, f.get_hit_damage(pid, type, st));
	return 1;
}

static int cget_hit_damage(plid pid, unsigned type, struct State *st) {
	int ret;

	(void)st;
	lua_getglobal(l, "get_hit_damage");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, type);

	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("get_hit_damage: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("get_hit_damage: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lget_effective_max_players(lua_State *l) {
	(void)l;

	lua_pushnumber(l, f.get_effective_max_players(st));
	return 1;
}

static plid cget_effective_max_players(struct State *st) {
	plid ret;

	(void)st;
	lua_getglobal(l, "get_effective_max_players");


	if (lua_pcall(l, 0, 1, 0) != 0)
		CBAILN1("get_effective_max_players: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("get_effective_max_players: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lget_anon_pid(lua_State *l) {
	(void)l;

	lua_pushnumber(l, f.get_anon_pid(st));
	return 1;
}

static plid cget_anon_pid(struct State *st) {
	plid ret;

	(void)st;
	lua_getglobal(l, "get_anon_pid");


	if (lua_pcall(l, 0, 1, 0) != 0)
		CBAILN1("get_anon_pid: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("get_anon_pid: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lafter_player_destroy(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.after_player_destroy(pid, st);
	return 0;
}

static void cafter_player_destroy(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "after_player_destroy");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("after_player_destroy: %s", luaL_checkstring(l, -1));
}


static int lbefore_estimated_fire(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.before_estimated_fire(pid, st);
	return 0;
}

static void cbefore_estimated_fire(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "before_estimated_fire");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("before_estimated_fire: %s", luaL_checkstring(l, -1));
}


static int lon_game_end(lua_State *l) {
	(void)l;

	f.on_game_end(st);
	return 0;
}

static void con_game_end(struct State *st) {
	(void)st;
	lua_getglobal(l, "on_game_end");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("on_game_end: %s", luaL_checkstring(l, -1));
}


static int lon_shutdown(lua_State *l) {
	(void)l;

	f.on_shutdown(st);
	return 0;
}

static void con_shutdown(struct State *st) {
	(void)st;
	lua_getglobal(l, "on_shutdown");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("on_shutdown: %s", luaL_checkstring(l, -1));
}


static int lbefore_log(lua_State *l) {
	(void)l;

	f.before_log(st);
	return 0;
}

static void cbefore_log(struct State *st) {
	(void)st;
	lua_getglobal(l, "before_log");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("before_log: %s", luaL_checkstring(l, -1));
}


static int lafter_log(lua_State *l) {
	(void)l;

	f.after_log(st);
	return 0;
}

static void cafter_log(struct State *st) {
	(void)st;
	lua_getglobal(l, "after_log");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("after_log: %s", luaL_checkstring(l, -1));
}


static int lon_masterlist_successful_connect(lua_State *l) {
	uint32_t peerid = luaL_checknumber(l, 1);

	f.on_masterlist_successful_connect(peerid, st);
	return 0;
}

static void con_masterlist_successful_connect(uint32_t peerid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_masterlist_successful_connect");

	lua_pushnumber(l, peerid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_masterlist_successful_connect: %s", luaL_checkstring(l, -1));
}


static int lon_masterlist_reconnect_attempt(lua_State *l) {
	uint32_t peerid = luaL_checknumber(l, 1);

	f.on_masterlist_reconnect_attempt(peerid, st);
	return 0;
}

static void con_masterlist_reconnect_attempt(uint32_t peerid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_masterlist_reconnect_attempt");

	lua_pushnumber(l, peerid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_masterlist_reconnect_attempt: %s", luaL_checkstring(l, -1));
}


static int lon_masterlist_disconnect(lua_State *l) {
	uint32_t peerid = luaL_checknumber(l, 1);

	f.on_masterlist_disconnect(peerid, st);
	return 0;
}

static void con_masterlist_disconnect(uint32_t peerid, struct State *st) {
	(void)st;
	lua_getglobal(l, "on_masterlist_disconnect");

	lua_pushnumber(l, peerid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("on_masterlist_disconnect: %s", luaL_checkstring(l, -1));
}


static int ltick(lua_State *l) {
	(void)l;

	f.tick(st);
	return 0;
}

static void ctick(struct State *st) {
	(void)st;
	lua_getglobal(l, "tick");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("tick: %s", luaL_checkstring(l, -1));
}


static int lload_initial_map(lua_State *l) {
	(void)l;

	f.load_initial_map(st);
	return 0;
}

static void cload_initial_map(struct State *st) {
	(void)st;
	lua_getglobal(l, "load_initial_map");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("load_initial_map: %s", luaL_checkstring(l, -1));
}


static int lclear_map(lua_State *l) {
	(void)l;

	f.clear_map(st);
	return 0;
}

static void cclear_map(struct State *st) {
	(void)st;
	lua_getglobal(l, "clear_map");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("clear_map: %s", luaL_checkstring(l, -1));
}


static int lprepare_map_load(lua_State *l) {
	(void)l;

	f.prepare_map_load(st);
	return 0;
}

static void cprepare_map_load(struct State *st) {
	(void)st;
	lua_getglobal(l, "prepare_map_load");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("prepare_map_load: %s", luaL_checkstring(l, -1));
}


static int lfinish_map_load(lua_State *l) {
	(void)l;

	f.finish_map_load(st);
	return 0;
}

static void cfinish_map_load(struct State *st) {
	(void)st;
	lua_getglobal(l, "finish_map_load");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("finish_map_load: %s", luaL_checkstring(l, -1));
}


static int lload_vxl_from_file(lua_State *l) {
	const char *path = luaL_checkstring(l, 1);

	lua_pushnumber(l, f.load_vxl_from_file(path, st));
	return 1;
}

static int cload_vxl_from_file(const char *path, struct State *st) {
	int ret;

	(void)st;
	lua_getglobal(l, "load_vxl_from_file");

	lua_pushstring(l, path);

	if (lua_pcall(l, 1, 1, 0) != 0)
		CBAILN1("load_vxl_from_file: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("load_vxl_from_file: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lbegin_load_vxl_from_file(lua_State *l) {
	const char *path = luaL_checkstring(l, 1);

	lua_pushnumber(l, f.begin_load_vxl_from_file(path, st));
	return 1;
}

static int cbegin_load_vxl_from_file(const char *path, struct State *st) {
	int ret;

	(void)st;
	lua_getglobal(l, "begin_load_vxl_from_file");

	lua_pushstring(l, path);

	if (lua_pcall(l, 1, 1, 0) != 0)
		CBAILN1("begin_load_vxl_from_file: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("begin_load_vxl_from_file: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lload_map(lua_State *l) {
	const char *name = luaL_checkstring(l, 1);

	lua_pushnumber(l, f.load_map(name, st));
	return 1;
}

static int cload_map(const char *name, struct State *st) {
	int ret;

	(void)st;
	lua_getglobal(l, "load_map");

	lua_pushstring(l, name);

	if (lua_pcall(l, 1, 1, 0) != 0)
		CBAILN1("load_map: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("load_map: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lfinish_cull(lua_State *l) {
	(void)l;

	f.finish_cull(st);
	return 0;
}

static void cfinish_cull(struct State *st) {
	(void)st;
	lua_getglobal(l, "finish_cull");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("finish_cull: %s", luaL_checkstring(l, -1));
}


static int lblock_action_rm(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1, 1);
	unsigned type = luaL_checknumber(l, 2);
	nplid from = check_nplid(l, 3);

	lua_pushnumber(l, f.block_action_rm(pos, type, from, st));
	return 1;
}

static uint32_t cblock_action_rm(ivec3 pos, unsigned type, nplid from, struct State *st) {
	uint32_t ret;

	(void)st;
	lua_getglobal(l, "block_action_rm");

	push_ivec3(pos);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 1, 0) != 0)
		CBAILN1("block_action_rm: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("block_action_rm: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lblock_action_cull(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1, 1);
	uint32_t mask = luaL_checknumber(l, 2);

	f.block_action_cull(pos, mask, st);
	return 0;
}

static void cblock_action_cull(ivec3 pos, uint32_t mask, struct State *st) {
	(void)st;
	lua_getglobal(l, "block_action_cull");

	push_ivec3(pos);
	lua_pushnumber(l, mask);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("block_action_cull: %s", luaL_checkstring(l, -1));
}


static int lblock_action(lua_State *l) {
	ivec3 pos = get_ivec3(l, 1, 1);
	unsigned type = luaL_checknumber(l, 2);
	nplid from = check_nplid(l, 3);

	f.block_action(pos, type, from, st);
	return 0;
}

static void cblock_action(ivec3 pos, unsigned type, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "block_action");

	push_ivec3(pos);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("block_action: %s", luaL_checkstring(l, -1));
}


static int lblock_line(lua_State *l) {
	ivec3 start = get_ivec3(l, 1, 1);
	ivec3 end = get_ivec3(l, 2, 1);
	nplid from = check_nplid(l, 3);

	f.block_line(start, end, from, st);
	return 0;
}

static void cblock_line(ivec3 start, ivec3 end, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "block_line");

	push_ivec3(start);
	push_ivec3(end);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("block_line: %s", luaL_checkstring(l, -1));
}


static int lset_fog(lua_State *l) {
	color color;

	get_color2(l, 1, color);

	f.set_fog(color, st);
	return 0;
}

static void cset_fog(color color, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_fog");

	push_color(color);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("set_fog: %s", luaL_checkstring(l, -1));
}


static int ltick_player_physics(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	float timeDelta = luaL_checknumber(l, 2);

	f.tick_player_physics(pid, timeDelta, st);
	return 0;
}

static void ctick_player_physics(bplid pid, float timeDelta, struct State *st) {
	(void)st;
	lua_getglobal(l, "tick_player_physics");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, timeDelta);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("tick_player_physics: %s", luaL_checkstring(l, -1));
}


static int lremove_grenade(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	f.remove_grenade(index, st);
	return 0;
}

static void cremove_grenade(size_t index, struct State *st) {
	(void)st;
	lua_getglobal(l, "remove_grenade");

	lua_pushnumber(l, index);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("remove_grenade: %s", luaL_checkstring(l, -1));
}


static int ldetonate_grenade(lua_State *l) {
	size_t index = luaL_checknumber(l, 1);

	f.detonate_grenade(index, st);
	return 0;
}

static void cdetonate_grenade(size_t index, struct State *st) {
	(void)st;
	lua_getglobal(l, "detonate_grenade");

	lua_pushnumber(l, index);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("detonate_grenade: %s", luaL_checkstring(l, -1));
}


static int lboot_players_to_limbo(lua_State *l) {
	(void)l;

	f.boot_players_to_limbo(st);
	return 0;
}

static void cboot_players_to_limbo(struct State *st) {
	(void)st;
	lua_getglobal(l, "boot_players_to_limbo");


	if (lua_pcall(l, 0, 0, 0) != 0)
		CBAIL("boot_players_to_limbo: %s", luaL_checkstring(l, -1));
}


static int lregister_grenade(lua_State *l) {
	plid pid = check_plid(l, 1);
	teamid team = check_teamid(l, 2);
	fvec3 pos = get_fvec3(l, 3);
	fvec3 vel = get_fvec3(l, 4);
	clk fuse = check_clk(l, 5);

	lua_pushnumber(l, f.register_grenade(pid, team, pos, vel, fuse, st));
	return 1;
}

static size_t cregister_grenade(plid pid, teamid team, fvec3 pos, fvec3 vel, clk fuse, struct State *st) {
	size_t ret;

	(void)st;
	lua_getglobal(l, "register_grenade");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team+1);
	push_fvec3(pos);
	push_fvec3(vel);
	push_clk(l, fuse);

	if (lua_pcall(l, 5, 1, 0) != 0)
		CBAILN1("register_grenade: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("register_grenade: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lspawn_grenade(lua_State *l) {
	plid pid = check_plid(l, 1);
	teamid team = check_teamid(l, 2);
	fvec3 pos = get_fvec3(l, 3);
	fvec3 vel = get_fvec3(l, 4);
	clk fuse = check_clk(l, 5);

	lua_pushnumber(l, f.spawn_grenade(pid, team, pos, vel, fuse, st));
	return 1;
}

static size_t cspawn_grenade(plid pid, teamid team, fvec3 pos, fvec3 vel, clk fuse, struct State *st) {
	size_t ret;

	(void)st;
	lua_getglobal(l, "spawn_grenade");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team+1);
	push_fvec3(pos);
	push_fvec3(vel);
	push_clk(l, fuse);

	if (lua_pcall(l, 5, 1, 0) != 0)
		CBAILN1("spawn_grenade: %s", luaL_checkstring(l, -1));

	if (!lua_isnumber(l, -1))
		CBAIL1N1("spawn_grenade: should return a number");

	ret = lua_tonumber(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lserver_msg(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	const char *msg = luaL_checkstring(l, 2);

	f.server_msg(pid, msg, st);
	return 0;
}

static void cserver_msg(bplid pid, const char *msg, struct State *st) {
	(void)st;
	lua_getglobal(l, "server_msg");

	lua_pushnumber(l, pid);
	lua_pushstring(l, msg);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("server_msg: %s", luaL_checkstring(l, -1));
}


static int lplayer_msg(lua_State *l) {
	const char *msg = luaL_checkstring(l, 1);
	unsigned type = luaL_checknumber(l, 2);
	plid from = check_plid(l, 3);

	f.player_msg(msg, type, from, st);
	return 0;
}

static void cplayer_msg(const char *msg, unsigned type, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "player_msg");

	lua_pushstring(l, msg);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("player_msg: %s", luaL_checkstring(l, -1));
}


static int lsend_map(lua_State *l) {
	bplid pid = check_bplid(l, 1);

	f.send_map(pid, st);
	return 0;
}

static void csend_map(bplid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_map");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("send_map: %s", luaL_checkstring(l, -1));
}


static int lsend_state(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.send_state(pid, st);
	return 0;
}

static void csend_state(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_state");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("send_state: %s", luaL_checkstring(l, -1));
}


static int lsend_connected_players(lua_State *l) {
	bplid pid = check_bplid(l, 1);

	f.send_connected_players(pid, st);
	return 0;
}

static void csend_connected_players(bplid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_connected_players");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("send_connected_players: %s", luaL_checkstring(l, -1));
}


static int lsend_chat(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	const char *msg = luaL_checkstring(l, 2);
	unsigned type = luaL_checknumber(l, 3);
	nplid from = check_nplid(l, 4);

	f.send_chat(pid, msg, type, from, st);
	return 0;
}

static void csend_chat(bplid pid, const char *msg, unsigned type, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_chat");

	lua_pushnumber(l, pid);
	lua_pushstring(l, msg);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_chat: %s", luaL_checkstring(l, -1));
}


static int lsend_quirks(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	const char *data = luaL_checkstring(l, 2);
	size_t len = luaL_checknumber(l, 3);

	f.send_quirks(pid, data, len, st);
	return 0;
}

static void csend_quirks(bplid pid, const char *data, size_t len, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_quirks");

	lua_pushnumber(l, pid);
	lua_pushstring(l, data);
	lua_pushnumber(l, len);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_quirks: %s", luaL_checkstring(l, -1));
}


static int lsend_quirks_off(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	const char *data = luaL_checkstring(l, 2);
	size_t len = luaL_checknumber(l, 3);

	f.send_quirks_off(pid, data, len, st);
	return 0;
}

static void csend_quirks_off(bplid pid, const char *data, size_t len, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_quirks_off");

	lua_pushnumber(l, pid);
	lua_pushstring(l, data);
	lua_pushnumber(l, len);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_quirks_off: %s", luaL_checkstring(l, -1));
}


static int lsend_block_action(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	ivec3 pos = get_ivec3(l, 2, 1);
	unsigned type = luaL_checknumber(l, 3);
	nplid from = check_nplid(l, 4);

	f.send_block_action(pid, pos, type, from, st);
	return 0;
}

static void csend_block_action(bplid pid, ivec3 pos, unsigned type, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_block_action");

	lua_pushnumber(l, pid);
	push_ivec3(pos);
	lua_pushnumber(l, type);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_block_action: %s", luaL_checkstring(l, -1));
}


static int lsend_block_line(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	ivec3 start = get_ivec3(l, 2, 1);
	ivec3 end = get_ivec3(l, 3, 1);
	nplid from = check_nplid(l, 4);

	f.send_block_line(pid, start, end, from, st);
	return 0;
}

static void csend_block_line(bplid pid, ivec3 start, ivec3 end, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_block_line");

	lua_pushnumber(l, pid);
	push_ivec3(start);
	push_ivec3(end);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_block_line: %s", luaL_checkstring(l, -1));
}


static int lsend_set_block_color(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	color color;
	nplid from = check_nplid(l, 3);

	get_color2(l, 2, color);

	f.send_set_block_color(pid, color, from, st);
	return 0;
}

static void csend_set_block_color(bplid pid, color color, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_set_block_color");

	lua_pushnumber(l, pid);
	push_color(color);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_set_block_color: %s", luaL_checkstring(l, -1));
}


static int lsend_set_tool(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned tool = luaL_checknumber(l, 2);
	plid from = check_plid(l, 3);

	f.send_set_tool(pid, tool, from, st);
	return 0;
}

static void csend_set_tool(bplid pid, unsigned tool, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_set_tool");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, tool);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_set_tool: %s", luaL_checkstring(l, -1));
}


static int lsend_player_update(lua_State *l) {
	bplid pid = check_bplid(l, 1);

	f.send_player_update(pid, st);
	return 0;
}

static void csend_player_update(bplid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_player_update");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("send_player_update: %s", luaL_checkstring(l, -1));
}


static int lsend_orientation(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 ori = get_fvec3(l, 2);

	f.send_orientation(pid, ori, st);
	return 0;
}

static void csend_orientation(bplid pid, fvec3 ori, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_orientation");

	lua_pushnumber(l, pid);
	push_fvec3(ori);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_orientation: %s", luaL_checkstring(l, -1));
}


static int lsend_position(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.send_position(pid, pos, st);
	return 0;
}

static void csend_position(bplid pid, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_position");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_position: %s", luaL_checkstring(l, -1));
}


static int lsend_reload(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned mag = luaL_checknumber(l, 2);
	unsigned reserve = luaL_checknumber(l, 3);
	nplid from = check_nplid(l, 4);

	f.send_reload(pid, mag, reserve, from, st);
	return 0;
}

static void csend_reload(bplid pid, unsigned mag, unsigned reserve, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_reload");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, mag);
	lua_pushnumber(l, reserve);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_reload: %s", luaL_checkstring(l, -1));
}


static int lsend_intel_capture(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	bint winning = lua_toboolean(l, 2);
	nplid from = check_nplid(l, 3);

	f.send_intel_capture(pid, winning, from, st);
	return 0;
}

static void csend_intel_capture(bplid pid, bint winning, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_intel_capture");

	lua_pushnumber(l, pid);
	lua_pushboolean(l, winning);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_intel_capture: %s", luaL_checkstring(l, -1));
}


static int lsend_intel_pickup(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	nplid from = check_nplid(l, 2);

	f.send_intel_pickup(pid, from, st);
	return 0;
}

static void csend_intel_pickup(bplid pid, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_intel_pickup");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_intel_pickup: %s", luaL_checkstring(l, -1));
}


static int lsend_intel_drop(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 pos = get_fvec3(l, 2);
	nplid from = check_nplid(l, 3);

	f.send_intel_drop(pid, pos, from, st);
	return 0;
}

static void csend_intel_drop(bplid pid, fvec3 pos, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_intel_drop");

	lua_pushnumber(l, pid);
	push_fvec3(pos);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_intel_drop: %s", luaL_checkstring(l, -1));
}


static int lsend_restock(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	nplid from = check_nplid(l, 2);

	f.send_restock(pid, from, st);
	return 0;
}

static void csend_restock(bplid pid, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_restock");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_restock: %s", luaL_checkstring(l, -1));
}


static int lsend_disconnect(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	nplid from = check_nplid(l, 2);

	f.send_disconnect(pid, from, st);
	return 0;
}

static void csend_disconnect(bplid pid, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_disconnect");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_disconnect: %s", luaL_checkstring(l, -1));
}


static int lsend_move_object(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 pos = get_fvec3(l, 2);
	unsigned id = luaL_checknumber(l, 3);
	gteamid team = check_gteamid(l, 4);

	f.send_move_object(pid, pos, id, team, st);
	return 0;
}

static void csend_move_object(bplid pid, fvec3 pos, unsigned id, gteamid team, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_move_object");

	lua_pushnumber(l, pid);
	push_fvec3(pos);
	lua_pushnumber(l, id);
	lua_pushnumber(l, team+1);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("send_move_object: %s", luaL_checkstring(l, -1));
}


static int lsend_map_start(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned size = luaL_checknumber(l, 2);

	f.send_map_start(pid, size, st);
	return 0;
}

static void csend_map_start(bplid pid, unsigned size, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_map_start");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, size);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_map_start: %s", luaL_checkstring(l, -1));
}


static int lsend_fog(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	color color;

	get_color2(l, 2, color);

	f.send_fog(pid, color, st);
	return 0;
}

static void csend_fog(bplid pid, color color, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_fog");

	lua_pushnumber(l, pid);
	push_color(color);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("send_fog: %s", luaL_checkstring(l, -1));
}


static int lsend_existing_player(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned team = luaL_checknumber(l, 2);
	unsigned gun = luaL_checknumber(l, 3);
	unsigned tool = luaL_checknumber(l, 4);
	unsigned score = luaL_checknumber(l, 5);
	color blockColor;
	const char *name = luaL_checkstring(l, 7);
	plid from = check_plid(l, 8);

	get_color2(l, 6, blockColor);

	f.send_existing_player(pid, team, gun, tool, score, blockColor, name, from, st);
	return 0;
}

static void csend_existing_player(bplid pid, unsigned team, unsigned gun, unsigned tool, unsigned score, color blockColor, const char *name, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_existing_player");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, team);
	lua_pushnumber(l, gun);
	lua_pushnumber(l, tool);
	lua_pushnumber(l, score);
	push_color(blockColor);
	lua_pushstring(l, name);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 8, 0, 0) != 0)
		CBAIL("send_existing_player: %s", luaL_checkstring(l, -1));
}


static int lsend_move_input(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned inputs = luaL_checknumber(l, 2);
	plid from = check_plid(l, 3);

	f.send_move_input(pid, inputs, from, st);
	return 0;
}

static void csend_move_input(bplid pid, unsigned inputs, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_move_input");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, inputs);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_move_input: %s", luaL_checkstring(l, -1));
}


static int lsend_mouse_input(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	unsigned inputs = luaL_checknumber(l, 2);
	plid from = check_plid(l, 3);

	f.send_mouse_input(pid, inputs, from, st);
	return 0;
}

static void csend_mouse_input(bplid pid, unsigned inputs, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_mouse_input");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, inputs);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("send_mouse_input: %s", luaL_checkstring(l, -1));
}


static int lsend_kill(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	clk spawndelta = check_clk(l, 2);
	unsigned type = luaL_checknumber(l, 3);
	plid killer = check_plid(l, 4);
	plid from = check_plid(l, 5);

	f.send_kill(pid, spawndelta, type, killer, from, st);
	return 0;
}

static void csend_kill(bplid pid, clk spawndelta, unsigned type, plid killer, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_kill");

	lua_pushnumber(l, pid);
	push_clk(l, spawndelta);
	lua_pushnumber(l, type);
	lua_pushnumber(l, killer);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 5, 0, 0) != 0)
		CBAIL("send_kill: %s", luaL_checkstring(l, -1));
}


static int lsend_grenade(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 pos = get_fvec3(l, 2);
	fvec3 vel = get_fvec3(l, 3);
	float fuse = luaL_checknumber(l, 4);
	nplid from = check_nplid(l, 5);

	f.send_grenade(pid, pos, vel, fuse, from, st);
	return 0;
}

static void csend_grenade(bplid pid, fvec3 pos, fvec3 vel, float fuse, nplid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_grenade");

	lua_pushnumber(l, pid);
	push_fvec3(pos);
	push_fvec3(vel);
	lua_pushnumber(l, fuse);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 5, 0, 0) != 0)
		CBAIL("send_grenade: %s", luaL_checkstring(l, -1));
}


static int lsend_spawn_player(lua_State *l) {
	bplid pid = check_bplid(l, 1);
	fvec3 pos = get_fvec3(l, 2);
	unsigned gun = luaL_checknumber(l, 3);
	nteamid team = check_nteamid(l, 4);
	const char *name = luaL_checkstring(l, 5);
	plid from = check_plid(l, 6);

	f.send_spawn_player(pid, pos, gun, team, name, from, st);
	return 0;
}

static void csend_spawn_player(bplid pid, fvec3 pos, unsigned gun, nteamid team, const char *name, plid from, struct State *st) {
	(void)st;
	lua_getglobal(l, "send_spawn_player");

	lua_pushnumber(l, pid);
	push_fvec3(pos);
	lua_pushnumber(l, gun);
	lua_pushnumber(l, team+1);
	lua_pushstring(l, name);
	lua_pushnumber(l, from);

	if (lua_pcall(l, 6, 0, 0) != 0)
		CBAIL("send_spawn_player: %s", luaL_checkstring(l, -1));
}


static int lspawn_player(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.spawn_player(pid, pos, st);
	return 0;
}

static void cspawn_player(plid pid, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "spawn_player");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("spawn_player: %s", luaL_checkstring(l, -1));
}


static int lreload_player(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.reload_player(pid, st);
	return 0;
}

static void creload_player(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "reload_player");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("reload_player: %s", luaL_checkstring(l, -1));
}


static int lrestock(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.restock(pid, st);
	return 0;
}

static void crestock(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "restock");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("restock: %s", luaL_checkstring(l, -1));
}


static int ldemand_fingerprint(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.demand_fingerprint(pid, st);
	return 0;
}

static void cdemand_fingerprint(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "demand_fingerprint");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("demand_fingerprint: %s", luaL_checkstring(l, -1));
}


static int lkill(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned type = luaL_checknumber(l, 2);
	plid killer = check_plid(l, 3);

	f.kill(pid, type, killer, st);
	return 0;
}

static void ckill(plid pid, unsigned type, plid killer, struct State *st) {
	(void)st;
	lua_getglobal(l, "kill");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, type);
	lua_pushnumber(l, killer);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("kill: %s", luaL_checkstring(l, -1));
}


static int lset_ammo(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned mag = luaL_checknumber(l, 2);
	unsigned reserve = luaL_checknumber(l, 3);

	f.set_ammo(pid, mag, reserve, st);
	return 0;
}

static void cset_ammo(plid pid, unsigned mag, unsigned reserve, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_ammo");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, mag);
	lua_pushnumber(l, reserve);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("set_ammo: %s", luaL_checkstring(l, -1));
}


static int lset_hp(lua_State *l) {
	plid pid = check_plid(l, 1);
	int hp = luaL_checknumber(l, 2);

	f.set_hp(pid, hp, st);
	return 0;
}

static void cset_hp(plid pid, int hp, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_hp");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, hp);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_hp: %s", luaL_checkstring(l, -1));
}


static int lset_hp_directional(lua_State *l) {
	plid pid = check_plid(l, 1);
	int hp = luaL_checknumber(l, 2);
	fvec3 pos = get_fvec3(l, 3);

	f.set_hp_directional(pid, hp, pos, st);
	return 0;
}

static void cset_hp_directional(plid pid, int hp, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_hp_directional");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, hp);
	push_fvec3(pos);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("set_hp_directional: %s", luaL_checkstring(l, -1));
}


static int ldamage_player(lua_State *l) {
	plid pid = check_plid(l, 1);
	int hp = luaL_checknumber(l, 2);
	unsigned type = luaL_checknumber(l, 3);
	plid damager = check_plid(l, 4);

	f.damage_player(pid, hp, type, damager, st);
	return 0;
}

static void cdamage_player(plid pid, int hp, unsigned type, plid damager, struct State *st) {
	(void)st;
	lua_getglobal(l, "damage_player");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, hp);
	lua_pushnumber(l, type);
	lua_pushnumber(l, damager);

	if (lua_pcall(l, 4, 0, 0) != 0)
		CBAIL("damage_player: %s", luaL_checkstring(l, -1));
}


static int ldamage_player_directional(lua_State *l) {
	plid pid = check_plid(l, 1);
	int hp = luaL_checknumber(l, 2);
	fvec3 pos = get_fvec3(l, 3);
	unsigned type = luaL_checknumber(l, 4);
	plid damager = check_plid(l, 5);

	f.damage_player_directional(pid, hp, pos, type, damager, st);
	return 0;
}

static void cdamage_player_directional(plid pid, int hp, fvec3 pos, unsigned type, plid damager, struct State *st) {
	(void)st;
	lua_getglobal(l, "damage_player_directional");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, hp);
	push_fvec3(pos);
	lua_pushnumber(l, type);
	lua_pushnumber(l, damager);

	if (lua_pcall(l, 5, 0, 0) != 0)
		CBAIL("damage_player_directional: %s", luaL_checkstring(l, -1));
}


static int lset_tool(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned tool = luaL_checknumber(l, 2);

	f.set_tool(pid, tool, st);
	return 0;
}

static void cset_tool(plid pid, unsigned tool, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_tool");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, tool);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_tool: %s", luaL_checkstring(l, -1));
}


static int lset_block_color(lua_State *l) {
	plid pid = check_plid(l, 1);
	color color;

	get_color2(l, 2, color);

	f.set_block_color(pid, color, st);
	return 0;
}

static void cset_block_color(plid pid, color color, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_block_color");

	lua_pushnumber(l, pid);
	push_color(color);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_block_color: %s", luaL_checkstring(l, -1));
}


static int lset_position(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.set_position(pid, pos, st);
	return 0;
}

static void cset_position(plid pid, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_position");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_position: %s", luaL_checkstring(l, -1));
}


static int lset_orientation(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 ori = get_fvec3(l, 2);

	f.set_orientation(pid, ori, st);
	return 0;
}

static void cset_orientation(plid pid, fvec3 ori, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_orientation");

	lua_pushnumber(l, pid);
	push_fvec3(ori);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("set_orientation: %s", luaL_checkstring(l, -1));
}


static int lset_jump(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.set_jump(pid, st);
	return 0;
}

static void cset_jump(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_jump");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("set_jump: %s", luaL_checkstring(l, -1));
}


static int lcapture_intel(lua_State *l) {
	plid pid = check_plid(l, 1);
	bint winning = lua_toboolean(l, 2);

	f.capture_intel(pid, winning, st);
	return 0;
}

static void ccapture_intel(plid pid, bint winning, struct State *st) {
	(void)st;
	lua_getglobal(l, "capture_intel");

	lua_pushnumber(l, pid);
	lua_pushboolean(l, winning);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("capture_intel: %s", luaL_checkstring(l, -1));
}


static int lpickup_intel(lua_State *l) {
	plid pid = check_plid(l, 1);

	f.pickup_intel(pid, st);
	return 0;
}

static void cpickup_intel(plid pid, struct State *st) {
	(void)st;
	lua_getglobal(l, "pickup_intel");

	lua_pushnumber(l, pid);

	if (lua_pcall(l, 1, 0, 0) != 0)
		CBAIL("pickup_intel: %s", luaL_checkstring(l, -1));
}


static int ldrop_intel(lua_State *l) {
	plid pid = check_plid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.drop_intel(pid, pos, st);
	return 0;
}

static void cdrop_intel(plid pid, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "drop_intel");

	lua_pushnumber(l, pid);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("drop_intel: %s", luaL_checkstring(l, -1));
}


static int lmove_intel(lua_State *l) {
	gteamid team = check_gteamid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.move_intel(team, pos, st);
	return 0;
}

static void cmove_intel(gteamid team, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "move_intel");

	lua_pushnumber(l, team+1);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("move_intel: %s", luaL_checkstring(l, -1));
}


static int lmove_tent(lua_State *l) {
	gteamid team = check_gteamid(l, 1);
	fvec3 pos = get_fvec3(l, 2);

	f.move_tent(team, pos, st);
	return 0;
}

static void cmove_tent(gteamid team, fvec3 pos, struct State *st) {
	(void)st;
	lua_getglobal(l, "move_tent");

	lua_pushnumber(l, team+1);
	push_fvec3(pos);

	if (lua_pcall(l, 2, 0, 0) != 0)
		CBAIL("move_tent: %s", luaL_checkstring(l, -1));
}


static int lhas_quirk(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned quirk = luaL_checknumber(l, 2);

	lua_pushboolean(l, f.has_quirk(pid, quirk, st));
	return 1;
}

static bint chas_quirk(plid pid, unsigned quirk, struct State *st) {
	bint ret;

	(void)st;
	lua_getglobal(l, "has_quirk");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, quirk);

	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("has_quirk: %s", luaL_checkstring(l, -1));

	if (!lua_isboolean(l, -1))
		CBAIL1N1("has_quirk: should return a boolean");

	ret = lua_toboolean(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lis_quirk_mutable(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned quirk = luaL_checknumber(l, 2);

	lua_pushboolean(l, f.is_quirk_mutable(pid, quirk, st));
	return 1;
}

static bint cis_quirk_mutable(plid pid, unsigned quirk, struct State *st) {
	bint ret;

	(void)st;
	lua_getglobal(l, "is_quirk_mutable");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, quirk);

	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("is_quirk_mutable: %s", luaL_checkstring(l, -1));

	if (!lua_isboolean(l, -1))
		CBAIL1N1("is_quirk_mutable: should return a boolean");

	ret = lua_toboolean(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lis_quirk_heuristic(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned quirk = luaL_checknumber(l, 2);

	lua_pushboolean(l, f.is_quirk_heuristic(pid, quirk, st));
	return 1;
}

static bint cis_quirk_heuristic(plid pid, unsigned quirk, struct State *st) {
	bint ret;

	(void)st;
	lua_getglobal(l, "is_quirk_heuristic");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, quirk);

	if (lua_pcall(l, 2, 1, 0) != 0)
		CBAILN1("is_quirk_heuristic: %s", luaL_checkstring(l, -1));

	if (!lua_isboolean(l, -1))
		CBAIL1N1("is_quirk_heuristic: should return a boolean");

	ret = lua_toboolean(l, -1);
	lua_pop(l, 1);
	return ret;
}


static int lset_quirk(lua_State *l) {
	plid pid = check_plid(l, 1);
	unsigned quirk = luaL_checknumber(l, 2);
	bint enabled = lua_toboolean(l, 3);

	f.set_quirk(pid, quirk, enabled, st);
	return 0;
}

static void cset_quirk(plid pid, unsigned quirk, bint enabled, struct State *st) {
	(void)st;
	lua_getglobal(l, "set_quirk");

	lua_pushnumber(l, pid);
	lua_pushnumber(l, quirk);
	lua_pushboolean(l, enabled);

	if (lua_pcall(l, 3, 0, 0) != 0)
		CBAIL("set_quirk: %s", luaL_checkstring(l, -1));
}

static void register_luaawk(lua_State *l, struct State *st) {
	(void)l;
	st->f.assign_new_pid = cassign_new_pid;
	st->f.on_any_connect = con_any_connect;
	st->f.on_successful_connect = con_successful_connect;
	st->f.on_disconnect = con_disconnect;
	st->f.on_join = con_join;
	st->f.on_switch = con_switch;
	st->f.on_position = con_position;
	st->f.on_orientation = con_orientation;
	st->f.on_move_input = con_move_input;
	st->f.on_mouse_input = con_mouse_input;
	st->f.on_color_change = con_color_change;
	st->f.on_block_action = con_block_action;
	st->f.on_block_line = con_block_line;
	st->f.on_chat = con_chat;
	st->f.on_tool_change = con_tool_change;
	st->f.on_hit = con_hit;
	st->f.on_grenade = con_grenade;
	st->f.on_reload = con_reload;
	st->f.on_quirks = con_quirks;
	st->f.on_handshake = con_handshake;
	st->f.get_spawn_time = cget_spawn_time;
	st->f.get_hit_damage = cget_hit_damage;
	st->f.get_effective_max_players = cget_effective_max_players;
	st->f.get_anon_pid = cget_anon_pid;
	st->f.after_player_destroy = cafter_player_destroy;
	st->f.before_estimated_fire = cbefore_estimated_fire;
	st->f.on_game_end = con_game_end;
	st->f.on_shutdown = con_shutdown;
	st->f.before_log = cbefore_log;
	st->f.after_log = cafter_log;
	st->f.on_masterlist_successful_connect = con_masterlist_successful_connect;
	st->f.on_masterlist_reconnect_attempt = con_masterlist_reconnect_attempt;
	st->f.on_masterlist_disconnect = con_masterlist_disconnect;
	st->f.tick = ctick;
	st->f.load_initial_map = cload_initial_map;
	st->f.clear_map = cclear_map;
	st->f.prepare_map_load = cprepare_map_load;
	st->f.finish_map_load = cfinish_map_load;
	st->f.load_vxl_from_file = cload_vxl_from_file;
	st->f.begin_load_vxl_from_file = cbegin_load_vxl_from_file;
	st->f.load_map = cload_map;
	st->f.finish_cull = cfinish_cull;
	st->f.block_action_rm = cblock_action_rm;
	st->f.block_action_cull = cblock_action_cull;
	st->f.block_action = cblock_action;
	st->f.block_line = cblock_line;
	st->f.set_fog = cset_fog;
	st->f.tick_player_physics = ctick_player_physics;
	st->f.remove_grenade = cremove_grenade;
	st->f.detonate_grenade = cdetonate_grenade;
	st->f.boot_players_to_limbo = cboot_players_to_limbo;
	st->f.register_grenade = cregister_grenade;
	st->f.spawn_grenade = cspawn_grenade;
	st->f.server_msg = cserver_msg;
	st->f.player_msg = cplayer_msg;
	st->f.send_map = csend_map;
	st->f.send_state = csend_state;
	st->f.send_connected_players = csend_connected_players;
	st->f.send_chat = csend_chat;
	st->f.send_quirks = csend_quirks;
	st->f.send_quirks_off = csend_quirks_off;
	st->f.send_block_action = csend_block_action;
	st->f.send_block_line = csend_block_line;
	st->f.send_set_block_color = csend_set_block_color;
	st->f.send_set_tool = csend_set_tool;
	st->f.send_player_update = csend_player_update;
	st->f.send_orientation = csend_orientation;
	st->f.send_position = csend_position;
	st->f.send_reload = csend_reload;
	st->f.send_intel_capture = csend_intel_capture;
	st->f.send_intel_pickup = csend_intel_pickup;
	st->f.send_intel_drop = csend_intel_drop;
	st->f.send_restock = csend_restock;
	st->f.send_disconnect = csend_disconnect;
	st->f.send_move_object = csend_move_object;
	st->f.send_map_start = csend_map_start;
	st->f.send_fog = csend_fog;
	st->f.send_existing_player = csend_existing_player;
	st->f.send_move_input = csend_move_input;
	st->f.send_mouse_input = csend_mouse_input;
	st->f.send_kill = csend_kill;
	st->f.send_grenade = csend_grenade;
	st->f.send_spawn_player = csend_spawn_player;
	st->f.spawn_player = cspawn_player;
	st->f.reload_player = creload_player;
	st->f.restock = crestock;
	st->f.demand_fingerprint = cdemand_fingerprint;
	st->f.kill = ckill;
	st->f.set_ammo = cset_ammo;
	st->f.set_hp = cset_hp;
	st->f.set_hp_directional = cset_hp_directional;
	st->f.damage_player = cdamage_player;
	st->f.damage_player_directional = cdamage_player_directional;
	st->f.set_tool = cset_tool;
	st->f.set_block_color = cset_block_color;
	st->f.set_position = cset_position;
	st->f.set_orientation = cset_orientation;
	st->f.set_jump = cset_jump;
	st->f.capture_intel = ccapture_intel;
	st->f.pickup_intel = cpickup_intel;
	st->f.drop_intel = cdrop_intel;
	st->f.move_intel = cmove_intel;
	st->f.move_tent = cmove_tent;
	st->f.has_quirk = chas_quirk;
	st->f.is_quirk_mutable = cis_quirk_mutable;
	st->f.is_quirk_heuristic = cis_quirk_heuristic;
	st->f.set_quirk = cset_quirk;
}

#define LUA_CALLS \
	{"assign_new_pid", lassign_new_pid}, \
	{"on_any_connect", lon_any_connect}, \
	{"on_successful_connect", lon_successful_connect}, \
	{"on_disconnect", lon_disconnect}, \
	{"on_join", lon_join}, \
	{"on_switch", lon_switch}, \
	{"on_position", lon_position}, \
	{"on_orientation", lon_orientation}, \
	{"on_move_input", lon_move_input}, \
	{"on_mouse_input", lon_mouse_input}, \
	{"on_color_change", lon_color_change}, \
	{"on_block_action", lon_block_action}, \
	{"on_block_line", lon_block_line}, \
	{"on_chat", lon_chat}, \
	{"on_tool_change", lon_tool_change}, \
	{"on_hit", lon_hit}, \
	{"on_grenade", lon_grenade}, \
	{"on_reload", lon_reload}, \
	{"on_quirks", lon_quirks}, \
	{"on_handshake", lon_handshake}, \
	{"get_spawn_time", lget_spawn_time}, \
	{"get_hit_damage", lget_hit_damage}, \
	{"get_effective_max_players", lget_effective_max_players}, \
	{"get_anon_pid", lget_anon_pid}, \
	{"after_player_destroy", lafter_player_destroy}, \
	{"before_estimated_fire", lbefore_estimated_fire}, \
	{"on_game_end", lon_game_end}, \
	{"on_shutdown", lon_shutdown}, \
	{"before_log", lbefore_log}, \
	{"after_log", lafter_log}, \
	{"on_masterlist_successful_connect", lon_masterlist_successful_connect}, \
	{"on_masterlist_reconnect_attempt", lon_masterlist_reconnect_attempt}, \
	{"on_masterlist_disconnect", lon_masterlist_disconnect}, \
	{"tick", ltick}, \
	{"load_initial_map", lload_initial_map}, \
	{"clear_map", lclear_map}, \
	{"prepare_map_load", lprepare_map_load}, \
	{"finish_map_load", lfinish_map_load}, \
	{"load_vxl_from_file", lload_vxl_from_file}, \
	{"begin_load_vxl_from_file", lbegin_load_vxl_from_file}, \
	{"load_map", lload_map}, \
	{"finish_cull", lfinish_cull}, \
	{"block_action_rm", lblock_action_rm}, \
	{"block_action_cull", lblock_action_cull}, \
	{"block_action", lblock_action}, \
	{"block_line", lblock_line}, \
	{"set_fog", lset_fog}, \
	{"tick_player_physics", ltick_player_physics}, \
	{"remove_grenade", lremove_grenade}, \
	{"detonate_grenade", ldetonate_grenade}, \
	{"boot_players_to_limbo", lboot_players_to_limbo}, \
	{"register_grenade", lregister_grenade}, \
	{"spawn_grenade", lspawn_grenade}, \
	{"server_msg", lserver_msg}, \
	{"player_msg", lplayer_msg}, \
	{"send_map", lsend_map}, \
	{"send_state", lsend_state}, \
	{"send_connected_players", lsend_connected_players}, \
	{"send_chat", lsend_chat}, \
	{"send_quirks", lsend_quirks}, \
	{"send_quirks_off", lsend_quirks_off}, \
	{"send_block_action", lsend_block_action}, \
	{"send_block_line", lsend_block_line}, \
	{"send_set_block_color", lsend_set_block_color}, \
	{"send_set_tool", lsend_set_tool}, \
	{"send_player_update", lsend_player_update}, \
	{"send_orientation", lsend_orientation}, \
	{"send_position", lsend_position}, \
	{"send_reload", lsend_reload}, \
	{"send_intel_capture", lsend_intel_capture}, \
	{"send_intel_pickup", lsend_intel_pickup}, \
	{"send_intel_drop", lsend_intel_drop}, \
	{"send_restock", lsend_restock}, \
	{"send_disconnect", lsend_disconnect}, \
	{"send_move_object", lsend_move_object}, \
	{"send_map_start", lsend_map_start}, \
	{"send_fog", lsend_fog}, \
	{"send_existing_player", lsend_existing_player}, \
	{"send_move_input", lsend_move_input}, \
	{"send_mouse_input", lsend_mouse_input}, \
	{"send_kill", lsend_kill}, \
	{"send_grenade", lsend_grenade}, \
	{"send_spawn_player", lsend_spawn_player}, \
	{"spawn_player", lspawn_player}, \
	{"reload_player", lreload_player}, \
	{"restock", lrestock}, \
	{"demand_fingerprint", ldemand_fingerprint}, \
	{"kill", lkill}, \
	{"set_ammo", lset_ammo}, \
	{"set_hp", lset_hp}, \
	{"set_hp_directional", lset_hp_directional}, \
	{"damage_player", ldamage_player}, \
	{"damage_player_directional", ldamage_player_directional}, \
	{"set_tool", lset_tool}, \
	{"set_block_color", lset_block_color}, \
	{"set_position", lset_position}, \
	{"set_orientation", lset_orientation}, \
	{"set_jump", lset_jump}, \
	{"capture_intel", lcapture_intel}, \
	{"pickup_intel", lpickup_intel}, \
	{"drop_intel", ldrop_intel}, \
	{"move_intel", lmove_intel}, \
	{"move_tent", lmove_tent}, \
	{"has_quirk", lhas_quirk}, \
	{"is_quirk_mutable", lis_quirk_mutable}, \
	{"is_quirk_heuristic", lis_quirk_heuristic}, \
	{"set_quirk", lset_quirk},

#endif
