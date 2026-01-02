-- apoc.lua -- Use /apoc to remove eardrums
local mod = {after={}};
local white = {r=255, g=255, b=255};
local white2 = {r=232, g=232, b=255};
local black = {r=32, g=24, b=16};
local black2 = {r=48, g=40, b=40};
local nexttick;
local modctr;
local lastprime;
local ctr;
local resetctr;
local nadepid;
local streak = {};
local apocs = {};
-- TODO: you can find a better way than "oldfog"
-- TODO: maybe send instead of set?
--local oldfog;

-- TODO: teamkill actually increases score for some reason
-- TODO on_unload stuff
function mod.on_load()
	nexttick = nil;
	for i in piditer(PID_BROADCAST) do
		streak[i] = 0;
		apocs[i] = 0;
	end
end

function mod.on_unload()
	destroy_strike();
	if (nexttick) then
		send_fog(PID_BROADCAST, get_fog());
	end
end

function mod.after.on_successful_connect(pid)
	streak[pid] = 0;
	apocs[pid] = 0;
end

-- Resetting on spawn instead of on_player_destroy gives last-second nades just a moment to churn before streak reset
-- NOTE: spectators can spawn too
function mod.after.spawn_player(pid)
	streak[pid] = 0;
end

function mod.after.boot_players_to_limbo()
	for i in piditer(PID_BROADCAST) do
		streak[i] = 0;
		apocs[i] = 0;
	end
end

function mod.after.on_disconnect(pid)
	local beststreak;

	-- TODO: base off of get_score(pid) instead of streak[pid]; streak is too easy to suddenly lose
	if (apocs[pid] > 0) then
		-- TODO: teamswitchers should probably not keep apocs, especially when switching to spectator
		for i in piditer(PID_BROADCAST_TEAM(get_team(pid))) do
			if (beststreak == nil or streak[i] > streak[beststreak]) then
				beststreak = i;
			end
		end

		if (beststreak ~= nil) then
			send_chat(PID_BROADCAST, get_name(pid).."'s apocs have been given to "..get_name(beststreak), 2, 0);
			apocs[beststreak] = apocs[beststreak] + apocs[pid];
		end
	end

	streak[pid] = nil;
	apocs[pid] = nil;
end

-- TODO: on_kill -> get_spawn_position, then add on_kill for on_hit but with a kill
-- would *not* be triggered by nade explosions though
function mod.after.kill(pid, type, killer)
	-- TODO: need easy way to determine if score increased

	-- This also prevents /kill from increasing streak.
	if (get_team(pid) ~= get_team(killer)) then
		streak[killer] = streak[killer] + 1;
		if (streak[killer] == 20) then
			apocs[killer] = apocs[killer] + 1;
			streak[killer] = 0;
			send_chat(PID_BROADCAST, get_name(killer).." can now use /apoc. . .", 2, 0);
		end
	end
end

local nadestart = {{x=256-49,  y=256-64}, {x=256-128,  y=256-64}};
local nadeend   = {{x=255+128, y=255+64}, {x=255+49, y=255+64}};

local function start_apoc(pid)
	nexttick = get_time() + 0.05;
	modctr = 0;
	--oldfog = get_fog();
	send_fog(PID_BROADCAST, black);
	ctr = 0;
	resetctr = 3;
	lastprime = 0;
	nadepid = pid;
end

local cmd = {name="apoc", desc="Summon an apoc if you have a 20 killstreak."};
function cmd.func(pid)
	if (apocs[pid] == 0) then
		send_chat(pid, "You need a 20 killstreak to use apoc! Current streak: "..tostring(streak[pid]), 2, 0);
		return;
	end

	if (nexttick ~= nil) then
		send_chat(pid, "Don't be so hasty!", 2, 0);
		return;
	end

	apocs[pid] = apocs[pid] - 1;
	start_apoc(pid);
end
register_command(cmd);

-- TODO: better way to signify "use the cmd: cap"
local cmd = {name="forceapoc", caps="", desc="Summon an apoc without bothering to check for killstreak."};
function cmd.func(pid)
	start_apoc(pid);
end
register_command(cmd);

local function is_prime(x)
	if (x % 2 == 0) then
		return false;
	end
	for i=3,x-1,2 do
		if (x % i == 0) then
			return false;
		end
	end
	return true;
end

-- TODO: remove all grenades on map load
local function get_impact_time(pos, vel)
	local time = 0;
	local hit;

	while (true) do
		pos, vel, hit = simulate_grenade_physics(pos, vel, 1/60);
		if (hit) then
			break;
		end
		time = time + 1/60;
	end

	return time;
end

local function lerp_colors(c1, c2, magnitude)
	local out = {};
	out.b = c1.b + (c2.b - c1.b) * magnitude;
	out.g = c1.g + (c2.g - c1.g) * magnitude;
	out.r = c1.r + (c2.r - c1.r) * magnitude;
	return out;
end

local function random_pos_from_team(team)
	local players = {};

	for x in piditer(PID_BROADCAST_TEAM(team)) do
		if (is_alive(x)) then
			table.insert(players, x);
		end
	end

	if (#players == 0) then
		return nil;
	end

	return get_position(players[math.random(#players)]);
end

-- TODO: cross() on forks?
-- TODO: loop around the map?
-- TODO: should lightning do damage?
-- TODO: should lightning target players/high things/water?
-- TODO: unhardcode map size?
-- TODO: if two forks intersect they trigger the is_solid part
-- TODO: add a team variable for the life of the apoc?
-- TODO: if you don't add spawn protection the enemy will use the player-targeting part to rip holes in your tower
local strikeblocks = {};
function build_strike(pos, forkchance)
	if (pos == nil) then
		if (math.random(0, 31) == 0) then
			--sc("!!!");
			pos = random_pos_from_team(get_team(nadepid) == 0 and 1 or 0);
		end
		if (pos == nil) then
			pos = {x=math.random(0,511), y=math.random(0,511)};
		end
		pos.z = 3;
		set_color(32, white);
		forkchance = 8;
	end

	--sc("START "..tostring(forks));
	while (true) do
		pos.x = pos.x % 512;
		pos.y = pos.y % 512;

		if (pos.z == 62 or is_solid(pos)) then
			-- TODO: grenades normally spawn next tick -- should nades with 0 fuse detonate immediately *without* intervention?
			detonate_grenade(spawn_grenade(nadepid, get_team(nadepid), {x=pos.x, y=pos.y, z=pos.z-1}, {x=0, y=0, z=0}, 0));
			detonate_grenade(spawn_grenade(nadepid, get_team(nadepid), {x=pos.x, y=pos.y, z=pos.z+1}, {x=0, y=0, z=0}, 0));
			break;
		end

		block_action(pos, 0, 32);
		-- TODO: how to deal with duplicates?
		table.insert(strikeblocks, {x=pos.x, y=pos.y, z=pos.z});
		--sc(string.format("{x=%u, y=%u, z=%02u}", pos.x, pos.y, pos.z));
		-- TODO: detect neighbors/solid and then don't go there
		-- TODO: add more skew to offshoots?

		if (math.random(0, 31) == 0) then
			--sc("offshoot at "..fmtval(pos));
			-- TODO: forks -> forkchance?
			-- Make a new table to ensure the old one doesn't get overwritten
			forkchance = forkchance * 2;
			build_strike({x=pos.x + math.random(-1, 1), y=pos.y + math.random(-1, 1), z=pos.z + 1}, forkchance);
		end
		if (math.random(0, 127) == 0) then
			break;
		end

		pos.x = pos.x + math.random(-1, 1);
		pos.y = pos.y + math.random(-1, 1);
		pos.z = pos.z + 1;
	end
	--sc("END "..tostring(forks));
end

function destroy_strike()
	for _,x in ipairs(strikeblocks) do
		block_action(x, 3, 0);
	end
	strikeblocks = {};
end

-- TODO: spectator apoc. . ?
function mod.after.tick()
	if (nexttick == nil or get_time() < nexttick) then
		return;
	end

	-- TODO: should it loop until all ticks are done?
	nexttick = nexttick + 0.05;
	ctr = ctr + 1;

	if (ctr >= 13/0.05) then
		send_fog(PID_BROADCAST, lerp_colors(black, get_fog(), ctr*0.05-13));
		-- TODO: normally i don't need to explicitly do this, but sometimes i do
		destroy_strike();

		if (ctr >= 14/0.05) then
			nexttick = nil;
			-- TODO: start_apoc() func
			--cmd.func(nadepid);
		end
		return;
	end

	-- TODO: handle disconnect, change team (specifically to spectator), etc.
	-- TODO: do i need to specify this team arg?
	-- TODO: can i make on_grenade call spawn_grenade?
	-- TODO: time grenades to detonate on impact
	-- TODO: get platform height from config?
	local team = get_team(nadepid);
	local nadepos = {x=math.random(nadestart[team+1].x, nadeend[team+1].x)+0.5, y=math.random(nadestart[team+1].y, nadeend[team+1].y)+0.5, z=-4};
	if (nadepos.x >= 256-50 and nadepos.x <= 255+50 and nadepos.y >= 256-16 and nadepos.y <= 255+16) then
		nadepos.z = 2;
	end
	local nadevel = {x=0, y=0, z=0.5};
	-- TODO: i'd like to recieve the grenade's ID and ensure it can't damage the origin player
	-- TODO: increase tickrate?
	spawn_grenade(nadepid, team, nadepos, nadevel, get_impact_time(nadepos, nadevel));
	spawn_grenade(nadepid, team, nadepos, nadevel, get_impact_time(nadepos, nadevel));

	if (resetctr == ctr) then
		send_fog(PID_BROADCAST, black2);
	end
	-- TODO: decouple from tickrate
	if (is_prime(ctr)) then
		--sc(ctr);
		if (modctr == 1) then
			send_fog(PID_BROADCAST, white2);
			build_strike();
			build_strike();
		elseif (modctr == 2) then
			-- TODO: don't send more packets than needed
			send_fog(PID_BROADCAST, black);
			destroy_strike();
		elseif (modctr == 3) then
			send_fog(PID_BROADCAST, black);
		elseif (modctr == 4) then
			send_fog(PID_BROADCAST, black);
		else
			send_fog(PID_BROADCAST, white);
			build_strike();
			build_strike();
			build_strike();
			build_strike();
			modctr = 0;
			resetctr = ctr + ctr - lastprime;
		end
		modctr = modctr + 1;
		lastprime = ctr;
	end
end

return mod;
