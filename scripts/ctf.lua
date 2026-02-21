-- ctf.lua -- Everyone's favorite gamemode
require "lib_l10n";
local mod = init_mod();
local drop_timeout = 0;

local not_holding_msg = {
	en="You're not holding the intel!"
};

local function lower_intel(team, loc)
	while (not is_solid{x=loc.x, y=loc.y, z=loc.z}) do
		-- TODO: dedup packets in core
		loc.z = loc.z + 1;
		move_intel(team-1, loc);
	end
end

local function raise_intel(team, loc)
	while (is_solid{x=loc.x, y=loc.y, z=loc.z}) do
		move_intel(team-1, loc);
		loc.z = loc.z - 1;
	end
end

local function lower_tent(team, loc)
	while (not is_solid{x=loc.x, y=loc.y, z=loc.z}) do
		loc.z = loc.z + 1;
		move_tent(team-1, loc);
	end
end

local function raise_tent(team, loc)
	while (is_solid{x=loc.x, y=loc.y, z=loc.z}) do
		move_tent(team-1, loc);
		loc.z = loc.z - 1;
	end
end

-- TODO: team += 1
-- Intel/tent gravity
function mod.after.finish_cull()
	local intelloc = get_intelloc();
	local tentloc = get_tentloc();

	for team, loc in pairs(intelloc) do
		lower_intel(team, loc);
	end

	for team, loc in pairs(tentloc) do
		lower_tent(team, loc);
	end
end

-- Intel/tent raise on block place
-- TODO: definitely test!!(?)
function mod.after.block_action(pos, type)
	if (type ~= 0) then
		return;
	end

	local intelloc = get_intelloc();
	local tentloc = get_tentloc();

	for team, loc in pairs(intelloc) do
		raise_intel(team, loc);
	end

	for team, loc in pairs(tentloc) do
		raise_tent(team, loc);
	end
end

-- TODO: raise/lower_tent()
function mod.after.block_line()
	local intelloc = get_intelloc();
	local tentloc = get_tentloc();

	for team, loc in pairs(intelloc) do
		raise_intel(team, loc);
	end

	for team, loc in pairs(tentloc) do
		while (is_solid{x=loc.x, y=loc.y, z=loc.z}) do
			-- TODO: dedup packets in core
			move_tent(team-1, loc);
			loc.z = loc.z - 1;
		end
	end
end

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
	if (get_team(pid) == 0) then
		lower_intel(2, {x=math.random(511-64, 511-64-63)+0.5, y=math.random(256-32, 255+32)+0.5, z=-1});
	else
		lower_intel(1, {x=math.random(64, 64+63)+0.5, y=math.random(256-32, 255+32)+0.5, z=-1});
	end
end

local function get_pintel(pid)
	local team = get_team(pid);
	if (team == 255) then
		return nil;
	end

	return get_intelloc()[(team == 1 and 0 or 1)+1];
end

-- TODO: get, set intel position
function mod.after.tick()
	local locs = get_intelloc();

	for team,intelloc in pairs(locs) do
		if (type(intelloc) == "number") then
			local tentloc = get_tentloc()[get_team(intelloc)+1];

			if (tentloc ~= nil and within_cylinder(get_position(intelloc), tentloc, 3, 1, -4)) then
				capture_intel(intelloc, get_team_score(get_team(intelloc))+1 >= 24);
			end
		elseif (get_time() >= drop_timeout) then for i in piditer(PID_BROADCAST) do
			if (is_alive(i) and get_team(i) ~= team-1 and within_cylinder(get_position(i), intelloc, 3, 1, -4)) then
				pickup_intel(i);
				break;
			end
		end end
	end
end

local function putback_intel()
	local intelloc = get_intelloc();

	-- TODO: drop dierctly on the spot?
	if (type(intelloc[1]) == "number") then
		drop_intel(intelloc[1], {x=math.huge, y=math.huge, z=math.huge});
	end

	if (type(intelloc[2]) == "number") then
		-- TODO: make drop_intel/move_intel accept nil
		drop_intel(intelloc[2], {x=math.huge, y=math.huge, z=math.huge});
	end

	lower_intel(1, {x=math.random(64, 64+63)+0.5, y=math.random(256-32, 255+32)+0.5, z=-1});
	lower_intel(2, {x=math.random(511-64, 511-64-63)+0.5, y=math.random(256-32, 255+32)+0.5, z=-1});

	lower_tent(1, {x=64+8, y=256, z=-1});
	lower_tent(2, {x=512-64-8, y=256, z=-1});
end

function mod.on_load()
	masterlist_set_gamemode("ctf");
	putback_intel();
end

function mod.before.finish_map_load()
	putback_intel();
end

-- TODO: hook drop_intel?
local function try_drop(pid)
	-- TODO: get_intel func that takes pid as arg?
	local intelloc = get_pintel(pid);

	if (intelloc == pid) then
		-- TODO: do i have to bounds check?
		local intelloc = get_position(intelloc);
		intelloc.x = math.floor(intelloc.x) + 0.5;
		intelloc.y = math.floor(intelloc.y) + 0.5;
		intelloc.z = math.ceil(intelloc.z);

		-- TODO: too many packets
		drop_intel(pid, intelloc);
		raise_intel(get_team(pid), intelloc);
		lower_intel(get_team(pid), intelloc);

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
	end
end
register_command(cmd);

function mod.after.after_player_destroy(pid)
	try_drop(pid);
end

return mod;
