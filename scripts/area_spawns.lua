-- area_spawns.lua -- Spawn within a predefined area
local mod = init_mod();

-- TODO: do something with the Z
getcfg("area_spawns", {
	{{x=      64, y=256-32, z=0}, {x=      64+63, y=255+32, z=0}},
	{{x=512 - 64, y=256-32, z=0}, {x=512 - 64-63, y=255+32, z=0}}
});

-- TODO: probably reuse objgrav
local function set_highest_point(pos)
	for z=0,63 do
		pos.z = z;

		if (is_solid(pos)) then
			return;
		end
	end

	pos.z = 64;
end

local function set_highest_point_spawn(pos)
	set_highest_point(pos);

	if (pos.z == 63) then
		pos.z = 64;
	end

	pos.z = pos.z - 2.251;
end

function mod.get_spawn_position(pid)
	local team = get_next_team(pid);

	if (team == SPECTATOR) then
		return mod.next.get_spawn_position(pid);
	end

	local area = area_spawns[team];
	local pos = {
		x=math.random(area[1].x, area[2].x),
		y=math.random(area[1].y, area[2].y),
		z=0,
	};

	set_highest_point_spawn(pos);

	return pos;
end

return mod;
