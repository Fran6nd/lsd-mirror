-- babel.lua -- The gamemode where nobody can cooperate
local mod = init_mod();
local drop_timeout = 0;
local platform_rebuild_time;

local platform_destroyed_msg = {
	en=">:("
};

local no_build_msg = {
	en="Can't do that there!",
};

local not_holding_msg = {
	en="You're not holding the intel!"
};

getcfg("babel_width", 100);
getcfg("babel_height", 32);
getcfg("babel_z", 1);

-- TODO: _override set of hooks? these would be all or nothing things, instead of passive listeners -- presumable the passives would come after the overrides?
-- TODO: or instead of dedicated override hooks, BETTER IDEA: just mark it as high-priority (override-priority?). . . and allow multiple hooks with different priorities in one module
-- TODO: on_kill?? on_kill -> get_spawn_time?
-- TODO: on_spawn -> get_spawn_pos?
-- TODO: deprovision gamemode stuff on unload
-- TODO: reprovision on load
-- TODO: what about for gamemode switch, i.e. ctf -> tc
-- TODO: expandable map size?
-- Size of the platform
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

local function raise_tents()
	for team, loc in pairs(get_tentloc()) do
		while (is_solid{x=loc.x, y=loc.y, z=loc.z}) do
			move_tent(team, loc);
			loc.z = loc.z - 1;
		end
	end
end

local function lower_tent(team, loc)
	while (loc.z < 63 and not is_solid{x=loc.x, y=loc.y, z=loc.z}) do
		loc.z = loc.z + 1;
		move_tent(team, loc);
	end
end

local function lower_tents()
	for team, loc in pairs(get_tentloc()) do
		lower_tent(team, loc);
	end
end

function mod.after.block_action(pos, type)
	if (type == 0) then
		raise_tents();
	end
end

function mod.after.block_line()
	raise_tents();
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

PID_COLOR_ANONYMOUS = 31
-- TODO: don't bother with building over solid stuff (unless it's a different color -- probably block over all on load but not platform destroy)
-- TODO: handle ridiculous blockaction queueing?
function mod.impl.babel_build_platform(mapload, pass2)
	set_block_color(PID_COLOR_ANONYMOUS, {b=255, g=255, r=0});

	if (mapload) then
		for y=plat_start.y,plat_end.y do
			for x=plat_start.x,plat_end.x do
				block_action({x=x, y=y, z=plat_z}, 0, PID_COLOR_ANONYMOUS);
			end
		end
	else
		for y=plat_start.y,plat_end.y do
			for x=plat_start.x,plat_end.x,50 do
				block_line({x=x, y=y, z=plat_z}, {x=math.min(x+49, plat_end.x), y=y, z=plat_z}, PID_COLOR_ANONYMOUS);
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
		babel_build_platform();
		-- Most of the clients don't process block line/action immediately when recieved.
		-- They do some cursed queueing thing that, for instance, lets you break and place
		-- a block on the same frame to recolor it. Anyway, those ones need some delay before
		-- rebuilding the platform.
		platform_rebuild_time = get_time()+0.1;
	end

	lower_tents();
end

function mod.early.on_block_action(pid, pos, type)
	if (not legal_pos(pos, type)) then
		l10n_send_chat(pid, no_build_msg);
		return;
	end

	mod.early.next.on_block_action(pid, pos, type);
end

-- TODO: block_line iterator func so i can hook on_block_line. . .

local function length2(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y);
end

local function within_cylinder(pos, cylinderpos, radius, bottom, top)
	pos.x = pos.x - cylinderpos.x;
	pos.y = pos.y - cylinderpos.y;
	pos.z = pos.z - cylinderpos.z;

	if (length2(pos) > radius or pos.z < top or pos.z > bottom) then
		return false;
	end

	return true;
end

-- TODO: probably take a team as arg instead? though, the score. . .
-- TODO: end game, also redo babel
function mod.after.capture_intel(pid)
	move_intel(1, {x=256, y=256, z=plat_z});
	local team = get_team(pid);
	if (team == 1) then
		move_intel(2, {x=math.huge, y=math.huge, z=math.huge});
	end
		-- TODO: special handling for intel 1,2 and their positions. . .
		-- TODO: probably don't need to move #2 back? except for recently-connected players. . .
		-- TODO: how do recently-connected players handle that?
end

local function get_1intel()
	local intelloc = get_intelloc();

	if (intelloc[1] == nil) then
		intelloc = intelloc[2];
	else
		intelloc = intelloc[1];
	end

	return intelloc;
end

-- TODO: get, set intel position
function mod.after.tick()
	local intelloc = get_1intel();

	if (platform_rebuild_time and get_time() >= platform_rebuild_time) then
		babel_build_platform(false, true);
		platform_rebuild_time = nil;
	end

	-- Don't do anything if someone is holding the intel
	-- TODO: do tents instead
	-- TODO: add restock.lua
	-- TODO: core tents
	if (type(intelloc) == "number") then
		local tentloc = get_tentloc()[get_team(intelloc)];
		if (tentloc ~= nil and within_cylinder(get_position(intelloc), tentloc, 3, 1, -4)) then
			-- TODO: make wrapper for capture_intel
			capture_intel(intelloc, get_team_score(get_team(intelloc))+1 >= get_max_score());
			-- TODO: put that intel back and maybe hook capture
			--intelloc = {x=256, y=256, z=1};
		end
	elseif (get_time() >= drop_timeout) then for i in piditer(PID_BROADCAST) do
		--print(intelloc);
		if (not is_alive(i)) then
			goto continue;
		end

		-- TODO: check if intel even exists
		-- TODO: z=1 or 0.5? does it even matter?
		-- TODO: if you port this to ctf, check team
		-- TODO: don't even bother with a global intel position, override that crap (nevermind, too complicated)
		-- TODO: provide send_state_{ctf,tc} func which has args for each thing
		-- TODO: babel can probably do with either more or less than 1 (maybe 0 or 2 or 3)
		if (within_cylinder(get_position(i), intelloc, 3, 1, -4)) then
			--print(i, "pickup", intelloc);
			pickup_intel(i);
			if (get_team(i) == 1) then
				move_intel(1, {x=math.huge, y=math.huge, z=math.huge});
			end
			break;
			-- TODO: hook pickup_intel?
			--intelloc = i;
		end

		::continue::
	end end
end

local function putback_intel()
	local intelloc = get_intelloc();
	if (type(intelloc[1]) == "number") then
		drop_intel(intelloc[1], {x=256, y=256, z=plat_z});
	else
		move_intel(1, {x=256, y=256, z=plat_z});
	end
	if (type(intelloc[2]) == "number") then
		-- TODO: make drop_intel/move_intel accept nil
		drop_intel(intelloc[2], {x=math.huge, y=math.huge, z=math.huge});
	else
		move_intel(2, {x=math.huge, y=math.huge, z=math.huge});
	end

	lower_tent(1, {x=128, y=256, z=-1});
	lower_tent(2, {x=512-128, y=256, z=-1});
end

-- TODO: don't build if server hasn't loaded a map
-- TODO: maybe on_hotload?
-- TODO: what happens if i load it *while* the map is loading?
function mod.on_load()
	masterlist_set_gamemode("babel");
	babel_build_platform();
	putback_intel();
end

-- TODO: intel position callback on map load?
-- TODO: hook after load and before send
function mod.before.finish_map_load()
	-- TODO: don't send packets for this. . .
	babel_build_platform(true);
	putback_intel();
end

local function try_drop(pid)
	local intelloc = get_1intel();
	if (intelloc == pid) then
		-- TODO: gravity for ctf
		-- TODO: what happens if the intel is dropped under the platform? should it always drop at player? only to a certain height? never?
		local intelloc = get_position(intelloc);
		intelloc.x = math.floor(intelloc.x) + 0.5;
		intelloc.y = math.floor(intelloc.y) + 0.5;
		--intelloc.z = math.ceil(intelloc.z);
		intelloc.z = plat_z;

		if (intelloc.x < plat_start.x or intelloc.x > plat_end.x+1 or
		    intelloc.y < plat_start.y or intelloc.y > plat_end.y+1) then
			intelloc.x = 256;
			intelloc.y = 256;
		end
		-- TODO: plumb all this junk into core already
		-- TODO: drop_intel func which takes only team or pid -- let the gamemode deal with it
		-- TODO: accept nil in place of fvec3
		drop_intel(pid, get_team(pid) == 2 and intelloc or {x=math.huge, y=math.huge, z=math.huge});
		if (get_team(pid) == 1) then
			move_intel(1, intelloc);
		end
		drop_timeout = get_time() + 2;
		return true;
	end

	return false;
end

-- TODO: allow dropping other players
local cmd = {name="drop", desc="Drop the intel if you're holding it."};
function cmd.func(pid)
	if (not try_drop(pid)) then
		l10n_send_chat(pid, not_holding_msg);
		-- TODO: add delay before intel pickup, maybe put in dedicated script
	end
end
register_command(cmd);

-- Drop intel on kill, disconnect, etc.
-- TODO: something seems very wrong about after.after_*
function mod.after.after_player_destroy(pid)
	try_drop(pid);
end

function mod.on_unload()
	-- TODO: you going to do something with this?
end

-- TODO: should babel hook the statedata and send its own cruft or depend on the server for that?
-- and what happens when we unload babel?
return mod;
