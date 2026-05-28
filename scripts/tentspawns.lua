-- tentspawns.lua -- Spawn randomly in an area around your team's tent
local mod = init_mod();

getcfg("tentspawns_radius", 20);

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

	-- TODO: just pass in team to get_tentloc(), not this table-returning crap
	local tentpos = get_tentloc()[team];
	if (tentpos == nil) then
		return mod.next.get_spawn_position(pid);
	end

	local pos = {
		x=math.random(math.floor(tentpos.x-tentspawns_radius), math.floor(tentpos.x-1+tentspawns_radius))+0.5,
		y=math.random(math.floor(tentpos.y-tentspawns_radius), math.floor(tentpos.y-1+tentspawns_radius))+0.5
	};

	set_highest_point_spawn(pos);

	return pos;
end

return mod;
