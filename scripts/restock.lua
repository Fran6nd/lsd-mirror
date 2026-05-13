-- restock.lua -- Refill ammo and things at a tent
local mod = init_mod();
local timeout = pid_spawn_table(0);

local function length2(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y);
end

-- TODO: near_tent call or something like that
local function within_cylinder(pos, cylinderpos, radius, bottom, top)
	pos.x = pos.x - cylinderpos.x;
	pos.y = pos.y - cylinderpos.y;
	pos.z = pos.z - cylinderpos.z;

	if (length2(pos) > radius or pos.z < top or pos.z > bottom) then
		return false;
	end

	return true;
end

function mod.after.tick()
	local now = get_time();

	for i in piditer(PID_BROADCAST) do
		if (is_alive(i) and within_cylinder(get_position(i), get_tentloc()[get_team(i)], 3, 1, -4) and now >= timeout[i]) then
			timeout[i] = now + 8;
			restock(i);
			-- TODO: only bother if there's something *to* restock
			-- TODO: allow aloha.pk babel-style ammo restocking (or maybe just add that as a hook to restock -- TODO: do that without sending 2 ammo packets in a row)
			-- TODO: probably just calculate diffs after every root event. . .
			-- TODO: and before any func which depends on those diffs
			-- TODO: and maybe before any lua send_ execution
			-- TODO: and maybe add a _now variant of non-send funcs
		end
	end
end

return mod;
