-- babel.lua -- The gamemode where nobody can cooperate
local mod = init_mod();
local drop_timeout = 0;
local platform_rebuild_time;

getcfg("babel_width", 100);
getcfg("babel_height", 32);
getcfg("babel_z",       1);
getcfg("babel_color", {b=255, g=255, r=0});

local platform_destroyed_msg = {
	en=">:("
};

local no_build_msg = {
	en="Can't do that there!",
};

local not_holding_msg = {
	en="You're not holding the intel!"
};

local plat_start = {x=256-babel_width/2, y=256-babel_height/2}
local plat_end = {x=255+babel_width/2, y=255+babel_height/2}
local plat_z = babel_z;

local function within(point, start, endp)
	return point >= start and point <= endp;
end

-- Within or adjacent.
local function adjacent(point, start, endp)
	return point >= start - 1 and point <= endp + 1;
end

local function on_corner(point, start, endp)
	return point == start - 1 or point == endp + 1;
end

local function within_xy(pos, start, endp)
	return within(pos.x, start.x, endp.x) and within(pos.y, start.y, endp.y);
end

local function adjacent_xy(pos, start, endp)
	return adjacent(pos.x, start.x, endp.x) and adjacent(pos.y, start.y, endp.y);
end

local function on_corner_xy(pos, start, endp, off)
	return on_corner(pos.x, start.x, endp.x) and on_corner(pos.y, start.y, endp.y);
end

local function legal_pos(pos, type)
	if (type == 0) then
		if (on_corner_xy(pos, plat_start, plat_end)) then
			return true;
		end

		if (pos.z == plat_z and adjacent_xy(pos, plat_start, plat_end)) then
			return false;
		end

		if (adjacent(pos.z, plat_z, plat_z) and within_xy(pos, plat_start, plat_end)) then
			return false;
		end
	end

	if (type == 1) then
		if (pos.z == plat_z and within_xy(pos, plat_start, plat_end)) then
			return false;
		end
	end

	if (type == 2) then
		if (adjacent(pos.z, plat_z, plat_z) and within_xy(pos, plat_start, plat_end)) then
			return false;
		end
	end

	if (type == 3) then
		if (adjacent(pos.z, plat_z, plat_z) and adjacent_xy(pos, plat_start, plat_end)) then
			return false;
		end
	end

	return true;
end

function mod.after.after_map_altered()
	-- intels are intentionally not gravitated
	-- TODO: probably make sure to chop off any debris
	-- lying on top of the platform that could
	-- do funny things
	gravitate_tents();
end

function mod.impl.babel_build_platform(mapload, pass2)
	set_block_color(get_anon_pid(), babel_color);

	if (mapload) then
		for y=plat_start.y,plat_end.y do
		for x=plat_start.x,plat_end.x do
			block_action({x=x, y=y, z=plat_z}, 0, get_anon_pid());
		end
		end
	else
		for y=plat_start.y,plat_end.y do
		for x=plat_start.x,plat_end.x,50 do
			block_line({x=x, y=y, z=plat_z}, {x=math.min(x+49, plat_end.x), y=y, z=plat_z}, get_anon_pid());
		end
		end
	end
end

-- Prevent most block actions from tearing down the platform
function mod.block_action_rm(pos, type, from)
	if (legal_pos(pos, type)) then
		return mod.next.block_action_rm(pos, type, from);
	end

	return 0;
end

-- Handle the few that tear it down anyway
function mod.after.finish_cull()
	if (not is_solid{x=plat_start.x, y=plat_start.y, z=plat_z}) then
		l10n_send_chat(PID_BROADCAST, platform_destroyed_msg);

		local clr = get_block_color(get_anon_pid());
		babel_build_platform();
		set_block_color(get_anon_pid(), clr);

		-- Most of the clients don't process block line/action immediately when recieved.
		-- They do some cursed queueing thing that, for instance, lets you break and place
		-- a block on the same frame to recolor it. Anyway, those ones need some delay before
		-- rebuilding the platform.
		platform_rebuild_time = get_time() + 0.1;
	end
end

function mod.early.on_block_action(pid, pos, type)
	if (not legal_pos(pos, type)) then
		l10n_send_chat(pid, no_build_msg);
		return;
	end

	mod.early.next.on_block_action(pid, pos, type);
end

-- Just to send "Can't do that there!" to people who throw nades
function mod.early.block_action(pos, type, pid)
	if (
		pid >= 0 and pid < MAX_PLAYERS and
		is_joined(pid) and
		type == 3 and
		not legal_pos(pos, type)
	) then
		l10n_send_chat(pid, no_build_msg);
		return;
	end

	mod.early.next.block_action(pos, type, pid);
end

function mod.early.on_block_line(pid, startp, endp)
	for pos in iter_block_line(startp, endp) do
		if (not legal_pos(pos, 0)) then
			l10n_send_chat(pid, no_build_msg);
			return;
		end
	end

	mod.early.next.on_block_line(pid, startp, endp);
end

function mod.after.capture_intel(pid)
	move_1intel{x=256, y=256, z=plat_z};
end

function mod.after.tick()
	local intelloc = get_1intel_loc();

	if (type(intelloc) == "number") then
		local tentloc = get_tentloc()[get_team(intelloc)];

		if (tent_in_range(get_position(intelloc), tentloc)) then
			capture_intel(
				intelloc,
				get_team_score(get_team(intelloc)) + 1
					>= get_max_score()
			);
		end

		return;
	end

	if (get_time() < drop_timeout) then
		return;
	end

	for i in piditer(PID_BROADCAST) do
		if (
			is_alive(i) and
			intel_in_range(get_position(i), intelloc)
		) then
			pickup_1intel(i);
			break;
		end
	end
end

local function putback_intel()
	drop_move_intel(1, {x=256, y=256, z=plat_z});
	drop_move_intel(2, {x=math.huge, y=math.huge, z=math.huge});

	gravitate_tent(1, {x=      128-10, y=256, z=0});
	gravitate_tent(2, {x=512 - 128+10, y=256, z=0});
end

function mod.on_load()
	masterlist_set_gamemode("babel");
	babel_build_platform();
	putback_intel();
end

function mod.before.finish_map_load()
	babel_build_platform(true);
	putback_intel();
end

local function clamp(min, val, max)
	return math.min(math.max(min, val), max);
end

local function clamp_position(pos)
	return {
		x=clamp(0, math.floor(pos.x), 511),
		y=clamp(0, math.floor(pos.y), 511),
		z=clamp(0, math.floor(pos.z),  63)
	};
end

local function try_drop(pid)
	if (not has_intel(pid)) then
		return false;
	end

	local pos = clamp_position(get_position(pid));

	if (not within_xy(pos, plat_start, plat_end)) then
		pos = {x=256, y=256, z=plat_z};
	end

	drop_1intel(pid, objgrav{
		x=math.floor(pos.x) + 0.5,
		y=math.floor(pos.y) + 0.5,
		z=plat_z
	});

	drop_timeout = get_time() + 2;

	return true;
end

-- TODO: allow dropping other players
local cmd = {name="drop", desc="Drop the intel if you're holding it."};
function cmd.func(pid)
	if (not try_drop(pid)) then
		l10n_send_chat(pid, not_holding_msg);
	end
end
register_command(cmd, mod);

function mod.after.after_player_destroy(pid)
	try_drop(pid);
end

return mod;
