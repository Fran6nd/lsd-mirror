-- onectf.lua -- Like babel but without the platform or the attempted cooperation
local mod = init_mod();
local drop_timeout = 0;

local not_holding_msg = {
	en="You're not holding the intel!"
};

function mod.after.after_map_altered()
	gravitate_intels();
	gravitate_tents();
end

function mod.after.capture_intel(pid)
	move_1intel(objgrav{x=256, y=256, z=0});
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
	drop_move_intel(1, objgrav{x=256, y=256, z=0});
	drop_move_intel(2, {x=math.huge, y=math.huge, z=math.huge});

	gravitate_tent(1, {x=      64+8, y=256, z=0});
	gravitate_tent(2, {x=512 - 64-8, y=256, z=0});
end

function mod.on_load()
	masterlist_set_gamemode("onectf");
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

	drop_1intel(pid, objgrav{
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
