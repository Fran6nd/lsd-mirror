-- arena.lua -- Not CTF
local mod = init_mod();
local gates = {};
local gatesdone = {};
local last_kill = {nil, nil};
local countdown_left = -1;
local next_countdown_tick;

local STAGE_WANDER = 0;
local STAGE_COUNTDOWN = 1;
local STAGE_GAME = 2;
local stage = STAGE_WANDER;

getcfg("arena_countdown_time", 5);

local start_msg = {
	en="Starting round in %(time) s."
}

-- Used for each tick of the countdown after the initial one.
local until_start_msg = start_msg;

function mod.get_spawn_position(pid)
	local meta = get_map_meta();
	local team = get_next_team(pid);

	if (team == 1) then
		if (meta.arena_blue_spawns) then
			return meta.arena_blue_spawns[math.random(#meta.arena_blue_spawns)];
		elseif (meta.arena_blue_spawn) then
			return meta.arena_blue_spawn;
		end
	elseif (team == 2) then
		if (meta.arena_green_spawns) then
			return meta.arena_green_spawns[math.random(#meta.arena_green_spawns)];
		elseif (meta.arena_green_spawn) then
			return meta.arena_green_spawn;
		end
	end

	-- hopefully just SPECTATOR
	return mod.next.get_spawn_position(pid);
end

function mod.get_spawn_time(pid)
	return 0;
end

function mod.after.kill(pid, type, killer)
	last_kill[get_team(killer)] = killer;

	-- TODO: make spawn_time <= get_time() instantly respawn?
	if (stage ~= STAGE_GAME) then
		spawn_player(pid, get_spawn_position(pid));
	end
end

local function add_gates()
	for _,x in ipairs(gates) do
		set_block_color(get_anon_pid(), x.color);
		block_action(x, 0, get_anon_pid());
	end
end

local function rm_gates()
	for _,x in ipairs(gates) do
		block_action(x, 1, get_anon_pid());
	end
end

local function begin_round()
	add_gates();

	next_countdown_tick = get_time()+1;
	countdown_left = arena_countdown_time;
	stage = STAGE_COUNTDOWN;

	last_kill = {nil, nil};

	for i in piditer(PID_BROADCAST) do
		if (is_joined(i) and get_team(i) ~= SPECTATOR) then
			spawn_player(i, get_spawn_position(i));
		end
	end

	l10n_send_chat(PID_BROADCAST, start_msg, {time=countdown_left});
end

-- Ugly >:(
initialMagAmmo = {
	[0]=10,
	30,
	6
};

initialReserveAmmo = {
	[0]=50,
	120,
	48
};

function mod.after.tick()
	if (stage == STAGE_COUNTDOWN and get_time() >= next_countdown_tick) then
		next_countdown_tick = next_countdown_tick + 1;
		countdown_left = countdown_left - 1;

		if (countdown_left == 0) then
			--l10n_send_chat(PID_BROADCAST, started_msg);
			stage = STAGE_GAME;

			for i in piditer(PID_BROADCAST) do
				if (is_alive(i)) then
					-- Refill any silly geese that use up their ammo before the round starts
					local gun = get_gun(i);

					-- TODO: certainly a better way than this
					restock(i);
					set_ammo(i, initialMagAmmo[gun], initialReserveAmmo[gun]);
				end
			end

			rm_gates();
			return;
		end

		l10n_send_chat(PID_BROADCAST, until_start_msg, {time=countdown_left});
	end
end

local function calc_players()
	local players = {0, 0};
	local alive = {0, 0};

	for i in piditer(PID_BROADCAST) do
		if (is_joined(i)) then
			local team = get_team(i);
			if (team ~= SPECTATOR) then
				players[team] = players[team] + 1;
				if (is_alive(i)) then
					alive[team] = alive[team] + 1;
				end
			end
		end
	end

	enough_players = players[1] > 0 and players[2] > 0;
	if (not enough_players) then
		-- Only countdown has gates
		if (stage == STAGE_COUNTDOWN) then
			rm_gates();
		end

		stage = STAGE_WANDER;
		return;
	end

	if (enough_players and stage == STAGE_WANDER) then
		begin_round();
		return;
	end

	for team,players in pairs(alive) do
		if (stage == STAGE_GAME and players == 0) then
			local winning_team = team == 1 and 2 or 1;
			local last = last_kill[winning_team];

			if (last == nil or not is_joined(last)) then
				for i in piditer(PID_BROADCAST) do
					if (is_alive(i) and get_team(i) == winning_team) then
						last = i;
						break;
					end
				end
			end

			capture_intel(last);
			begin_round();
		end
	end
end

function mod.after.spawn_player()
	calc_players();
end

function mod.after.after_player_destroy()
	calc_players();
end

function mod.after.on_join(pid)
	for team,i in pairs(last_kill) do
		if (i == pid) then
			last_kill[team] = nil;
		end
	end
end

-- TODO: convert vecs to just arrays?
local axismap = {"x","y","z"};
local function tuple_to_vec(scrape, str)
	local i = 0;
	local vec = {};

	for x in scrape.split_tuple(str) do
		i = i + 1;
		vec[axismap[i]] = tonumber(x);
	end

	return vec;
end
local function get_vec_tuple(scrape, meta, str, name)
	local i = 0;
	local str = scrape.get_ext(str, name);

	if (str) then
		meta[name] = tuple_to_vec(scrape, str);
	end
end

local function get_vec_tupletuple(scrape, meta, str, name)
	local str = scrape.get_ext(str, name);

	if (str) then
		meta[name] = {};
		for x in scrape.split_tupletuple(str) do
			table.insert(meta[name], tuple_to_vec(scrape, x));
		end
	end
end

function mod.after.pyscrape_ext(scrape, str, meta)
	meta.arena = scrape.parse_bool(scrape.get_ext(str, "arena"));
	get_vec_tuple(scrape, meta, str, "arena_blue_spawn");
	get_vec_tuple(scrape, meta, str, "arena_green_spawn");
	get_vec_tupletuple(scrape, meta, str, "arena_blue_spawns");
	get_vec_tupletuple(scrape, meta, str, "arena_green_spawns");

	get_vec_tupletuple(scrape, meta, str, "arena_gates");
end

-- TODO: to core, and deal with Z fuckery there -- in_bounds_vox/in_bounds_phys? what about nade phys? wrapping?
local function in_bounds(pos)
	return (pos.x >= 0 and pos.x < 512 and pos.y >= 0 and pos.y < 512 and pos.z >= 0 and pos.z < 64);
end

-- TODO: expose a modified version of cull.h?
-- TODO: expose a meta key that doesn't suck?
function floodfill_gate(pos)
	local tocheck = {};
	local color = get_map_block_color(pos);
	table.insert(tocheck, pos);

	while (#tocheck ~= 0) do
		local item = table.remove(tocheck);
		-- TODO: stop hardcoding :(
		local donekey = item.x+item.y*512+item.z*512*512;

		local itemcolor = get_map_block_color(item);
		local itemcolormatches = itemcolor.r == color.r and itemcolor.g == color.g and itemcolor.b == color.b;

		if (not gatesdone[donekey] and in_bounds(item) and is_solid(item) and itemcolormatches) then
			item.color = color;

			gatesdone[donekey] = true;

			table.insert(gates, item);

			table.insert(tocheck, {x=item.x-1,y=item.y  ,z=item.z  });
			table.insert(tocheck, {x=item.x+1,y=item.y  ,z=item.z  });
			table.insert(tocheck, {x=item.x  ,y=item.y-1,z=item.z  });
			table.insert(tocheck, {x=item.x  ,y=item.y+1,z=item.z  });
			table.insert(tocheck, {x=item.x  ,y=item.y  ,z=item.z-1});
			table.insert(tocheck, {x=item.x  ,y=item.y  ,z=item.z+1});
		end
	end
end

function mod.after.load_map()
	local meta = get_map_meta();
	gates = {};
	gatesdone = {};

	-- TODO: this treats map load success the same as failure -- puts you permanently-ish into wander on fail (then screws all the spawns up)
	stage = STAGE_WANDER;

	if (meta.arena_gates ~= nil) then
		for _,x in ipairs(meta.arena_gates) do
			floodfill_gate(x);
		end
	end

	-- TODO: move to during-after load?
	-- (maybe not actually)
	rm_gates();
end

return mod;
