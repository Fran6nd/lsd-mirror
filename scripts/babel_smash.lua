-- babel_smash.lua -- Wonder why they call it "smash"
local mod = init_mod();
local active = false;

-- "Borrows" babel_width, babel_height and babel_z from babel.lua
getcfg("babel_smash_dist", 16);

local function get_spawn_pos(i, ctr, total)
	local team = get_next_team(i);

	if (team == 1) then
		return {x=256.5-babel_width/2+babel_smash_dist, y=256-babel_height/2+babel_height*ctr/total, z=babel_z - 2.251};
	end

	return {x=255.5+babel_width/2-babel_smash_dist, y=256-babel_height/2+babel_height*ctr/total, z=babel_z - 2.251};
end

local function shuf(tbl)
	for i=#tbl,1,-1 do
		local swapi = math.random(i);
		local tmp = tbl[i];

		tbl[i] = tbl[swapi];
		tbl[swapi] = tmp;
	end
end

function mod.get_spawn_time(pid)
	if (active) then
		return 0;
	end

	return mod.next.get_spawn_time(pid);
end

function mod.after.end_disco()
	active = false;
end

function mod.after.start_disco(game_end)
	if (not game_end) then
		return;
	end

	active = true;

	-- total is off by one to prevent spawning directly on the platform edges
	local total = {1, 1}
	local ctr = {0, 0}
	local players = {};

	for i in piditer(PID_BROADCAST) do
		local team = get_next_team(i);

		if (is_joined(i) and team ~= SPECTATOR) then
			total[team] = total[team] + 1;
			table.insert(players, i);
		end
	end

	-- Randomize relative player positions instead of
	-- basing spawn position on pid
	shuf(players);

	for _,i in ipairs(players) do
		local team = get_next_team(i);

		ctr[team] = ctr[team] + 1;
		spawn_player(i, get_spawn_pos(i, ctr[team], total[team]));
	end
end

return mod;
