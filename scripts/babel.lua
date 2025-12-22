-- babel.lua -- The gamemode where nobody can cooperate
local mod = {};
local intelloc = {x=256, y=256, z=1};

-- TODO: team starts at 0 or 1?
-- TODO: i think some places use 0 and others use 1, unify that
function get_tent_position(team)
	local x = {};
	x[0] = {x=128, y=256, z=62};
	x[1] = {x=512-128, y=256, z=62};
	return x[team];
end

-- TODO: _override set of hooks? these would be all or nothing things, instead of passive listeners -- presumable the passives would come after the overrides?
-- TODO: or instead of dedicated override hooks, BETTER IDEA: just mark it as high-priority (override-priority?). . . and allow multiple hooks with different priorities in one module
-- TODO: on_kill?? on_kill -> get_spawn_time?
-- TODO: on_spawn -> get_spawn_pos?
-- TODO: deprovision gamemode stuff on unload
-- TODO: reprovision on load
-- TODO: what about for gamemode switch, i.e. ctf -> tc
-- TODO: expandable map size?
-- Size of the platform
local size = {x=100, y=32}
local plat_start = {x=256-size.x/2, y=256-size.y/2}
local plat_end = {x=255+size.x/2, y=255+size.y/2}
local plat_z = 1;

local function legal_pos(pos, type)
	-- TODO: discrepencies between 0, 1, 2
	if (type == 0 or type == 1) then
		if ((pos.x == plat_start.x - 1 or pos.x == plat_end.x + 1) and (pos.y == plat_start.y - 1 or pos.y == plat_end.y + 1)) then
			return true;
		end

		if (pos.z == plat_z and
		    pos.x >= plat_start.x - 1 and pos.x <= plat_end.x + 1 and
		    pos.y >= plat_start.y - 1 and pos.y <= plat_end.y + 1) then
			return false;
		end

		if (pos.z >= plat_z   - 1 and pos.z <= plat_z + 1 and
		    pos.x >= plat_start.x and pos.x <= plat_end.x and
		    pos.y >= plat_start.y and pos.y <= plat_end.y ) then
			return false;
		end
	end

	if (type == 2) then
		if ((pos.x == plat_start.x - 1 or pos.x == plat_end.x + 1) and (pos.y == plat_start.y - 1 or pos.y == plat_end.y + 1)) then
			return true;
		end

		if (pos.z >= plat_z   - 1 and pos.z <= plat_z + 1 and
		    pos.x >= plat_start.x and pos.x <= plat_end.x and
		    pos.y >= plat_start.y and pos.y <= plat_end.y ) then
			return false;
		end
	end

	if (type == 3) then
		if ((pos.x == plat_start.x - 2 or pos.x == plat_end.x + 2) and (pos.y == plat_start.y - 2 or pos.y == plat_end.y + 2)) then
			return true;
		end

		if (pos.z >= plat_z       - 1 and pos.z <= plat_z     + 1 and
		    pos.x >= plat_start.x - 1 and pos.x <= plat_end.x + 1 and
		    pos.y >= plat_start.y - 1 and pos.y <= plat_end.y + 1) then
			return false;
		end
	end

	return true;
end

-- TODO: don't bother with building over solid stuff (unless it's a different color -- probably block over all on load but not platform destroy)
-- TODO: handle ridiculous blockaction queueing?
local function build_platform()
	set_color(32, {b=255, g=255, r=0});

	for y=plat_start.y,plat_end.y do
		for x=plat_start.x,plat_end.x do
			block_action({x=x, y=y, z=plat_z}, 0, 32);
		end
	end
end

-- TODO: rename this trash
-- TODO: hook grenades instead
-- Try to prevent platform destruction
-- TODO: if platform gets nuked after one of these anyway, fix it
function mod.block_action(pos, type, from)
	--if (legal_pos(pos, type)) then
		--next_call("block_action", mod.block_action)(pos, type, from);
	--end
	next_call("block_action", mod.block_action)(pos, type, from);
end

-- TODO: don't do this stop_exec thing, hook into some cannot_do_this thing
-- TODO: i definitely agree with this, use something crap_packet-style (maybe use different return values to mean different things)
function mod.on_block_action(pid, pos, type)
	if (not legal_pos(pos, type)) then
		send_chat(pid, "Can't do that there!", 2, 0);
		stop_exec();
	end

	-- else?
	next_call("on_block_action", mod.on_block_action)(pid, pos, type);
end

function length2(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y);
end

function within_cylinder(pos, cylinderpos, radius, bottom, top)
	pos.x = pos.x - cylinderpos.x;
	pos.y = pos.y - cylinderpos.y;
	pos.z = pos.z - cylinderpos.z;

	if (length2(pos) > radius or pos.z < top or pos.z > bottom) then
		return false;
	end

	return true;
end

-- TODO: get, set intel position
function mod.tick()
	next_call("tick", mod.tick)();

	-- Don't do anything if someone is holding the intel
	-- TODO: do tents instead
	-- TODO: add restock.lua
	-- TODO: core tents
	-- TODO: add hook get_tent_position(pid, team, st)? yes, with a pid
	if (type(intelloc) == "number") then
		if (within_cylinder(get_position(intelloc), get_tent_position(get_team(intelloc)), 3, 1, -4)) then
			capture_intel(intelloc);
			-- TODO: put that intel back and maybe hook capture
			intelloc = {x=256, y=256, z=1};
		end
	else for i=0,MAX_PLAYERS-1 do
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
			pickup_intel(i);
			-- TODO: hook pickup_intel?
			intelloc = i;
		end

		::continue::
	end end
end

-- TODO: don't build if server hasn't loaded a map
-- TODO: maybe on_hotload?
-- TODO: what happens if i load it *while* the map is loading?
function mod.on_load()
	build_platform();
end

-- TODO: intel position callback on map load?
-- TODO: hook after load and before send
function mod.load_map_from_file(path)
	next_call("load_map_from_file", mod.load_map_from_file)(path);
	build_platform();
end

-- TODO: fog color should definitely be hooked into core probably
-- TODO: maybe there should just be a callback for getting gamemode trash
-- TODO: probably no callback, just provide the necessary ones you dunce. . .
-- need to get max_score at least though, and probably teamscore too
-- TODO: intel/tent pos config? well not intel on babel
function mod.send_state_ctf(pid, from, teamname, teamcolor, fog, teamscore, maxscore, _intelloc, tentpos)
	local loc2 = {};-- = {intelloc, intelloc};
	if (type(intelloc) == "number") then
		if (get_team(intelloc) == 0) then
			loc2[1] = intelloc;
		else
			loc2[2] = intelloc;
		end
	else
		loc2[1] = intelloc;
	end
	next_call("send_state_ctf", mod.send_state_ctf)(pid, from, teamname, teamcolor, fog, teamscore, maxscore, loc2, {get_tent_position(0), get_tent_position(1)});
end

function mod.kill(pid, type, by)
	next_call("kill", mod.kill)(pid, type, by);
	if (intelloc == pid) then
		-- TODO: gravity
		intelloc = get_position(intelloc);
		intelloc.x = math.floor(intelloc.x) + 0.5;
		intelloc.y = math.floor(intelloc.y) + 0.5;
		intelloc.z = math.ceil(intelloc.z);
		-- TODO: plumb all this junk into core already
		-- TODO: drop_intel func which takes only team or pid -- let the gamemode deal with it
		drop_intel(pid, intelloc);
	end
end

function mod.on_unload()

end

-- TODO: should babel hook the statedata and send its own cruft or depend on the server for that?
-- and what happens when we unload babel?
return mod;
