-- fall_damage.lua -- Like pyspades fall damage if it sucked less and also wasn't bad
local mod = init_mod();
local apex = {};

-- TODO: do i still need this after doing tickery?
function mod.on_load()
	for i in piditer(PID_BROADCAST) do
		if (is_alive(i)) then
			apex[i] = get_position(i).z;
		end
	end
end

-- TODO: do i need all these funcs just to detect position change?
function mod.after.spawn_player(pid)
	apex[pid] = get_position(pid).z;
end

function mod.after.set_position(pid, pos)
	-- TODO: wonder what happens if a further call in set_position changes the position
	-- TODO: should i even check for apex here or defer to tick?
	if (pos.z < apex[pid]) then
		apex[pid] = pos.z;
	end
end

-- TODO: what about player-triggered jumps if i ever upgrade its crap detector?
function mod.after.set_jump(pid)
	apex[pid] = get_position(pid).z;
end

-- Guesstimates the fall damage based roughly on demoncore physics
local function get_fall_damage(height)
	if (height > 250.14) then
		return 722;
	end

	local vel = 0;
	local z = 0;
	local tickdelta = 1/60;

	while (z < height) do
		vel = vel + tickdelta;
		vel = vel / (tickdelta + 1);
		z = z + vel * tickdelta * 32;
	end

	if (vel > 0.58) then
		vel = vel - 0.58;
		return math.floor(vel * vel * 4096);
	end

	return 0;
end

-- TODO: wonder how this behaves with noclip and tp's
-- TODO: should lua/player jump reset the apex?
-- TODO: crap packet tp vs manual tp. . ?
-- TODO: should fall damage/apex only deal with updating itself when demoncore tick is done?
-- TODO: should fall damage apply when not airborne too? (i.e. positiondata sent much lower)
function mod.tick()
	local was_airborne = {};

	for i in piditer(PID_BROADCAST) do
		if (is_alive(i)) then
			was_airborne[i] = is_airborne(i);
		end
	end

	mod.next.tick();

	for i in piditer(PID_BROADCAST) do
		if (is_alive(i)) then
			local pos = get_position(i);

			if (pos.z < apex[i]) then
				apex[i] = pos.z;
			end

			if (was_airborne[i] and not is_airborne(i)) then
				damage_player(i, get_fall_damage(math.abs(apex[i]-pos.z)), 4, i);
				apex[i] = pos.z;
			end
		end
	end
end

return mod;
