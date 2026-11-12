-- ctf.lua -- Everyone's favorite gamemode
local mod = init_mod();
local drop_timeout = 0;

local not_holding_msg = {
	en="You're not holding the intel!"
};

function mod.after.after_map_altered()
	gravitate_intels();
	gravitate_tents();
end

-- TODO: probably take a team as arg instead? though, the score. . .
-- TODO: end game, also redo babel
function mod.after.capture_intel(pid)
	if (get_team(pid) == 1) then
		gravitate_intel(2, {x=math.random(511-64, 511-64-63)+0.5, y=math.random(256-32, 255+32)+0.5, z=0});
	else
		gravitate_intel(1, {x=math.random(64, 64+63)+0.5, y=math.random(256-32, 255+32)+0.5, z=0});
	end
end

function mod.after.tick()
	local locs = get_intelloc();

	for team, intelloc in pairs(locs) do
		if (type(intelloc) == "number") then
			-- TODO: retrofit get_tentloc/get_intelloc to (optionally)
			-- take a team id and just return the pos for that team
			local tentloc = get_tentloc()[get_team(intelloc)];

			if (tent_in_range(get_position(intelloc), tentloc)) then
				capture_intel(
					intelloc,
					get_team_score(get_team(intelloc)) + 1
						>= get_max_score()
				);
			end
		elseif (get_time() >= drop_timeout) then
		for i in piditer(PID_BROADCAST) do
			if (
				is_alive(i) and
				get_team(i) ~= team and
				intel_in_range(get_position(i), intelloc)
			) then
				pickup_intel(i);
				break;
			end
		end
		end
	end
end

local function putback_intel()
	drop_move_intel(1, objgrav{
		x=math.random(      64,       64+63) + 0.5,
		y=math.random(256 - 32, 255 + 32)    + 0.5,
		z=0
	});

	drop_move_intel(2, objgrav{
		x=math.random(511 - 64, 511 - 64-63) + 0.5,
		y=math.random(256 - 32, 255 + 32)    + 0.5,
		z=0
	});

	gravitate_tent(1, {x=      64+8, y=256, z=0});
	gravitate_tent(2, {x=512 - 64-8, y=256, z=0});
end

function mod.on_load()
	masterlist_set_gamemode("ctf");
	putback_intel();
end

function mod.before.finish_map_load()
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

	drop_intel(pid, objgrav{
		x=math.floor(pos.x) + 0.5,
		y=math.floor(pos.y) + 0.5,
		z=math.ceil (pos.z)
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
