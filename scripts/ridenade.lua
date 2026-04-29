-- ridenade.lua -- Ride tossed grenades
local mod = init_mod();
-- TODOTODOTODO: need a table that clears on death. . .
local whichnade = pid_spawn_table(nil);

-- TODO: pass ret to mod.after.func?
function mod.register_grenade(pid, team, pos, vel, fuse)
	whichnade[pid] = mod.next.register_grenade(pid, team, pos, vel, fuse);
	return whichnade[pid];
end

function mod.before.remove_grenade(index)
	local pid = get_grenade_pid(index);

	if (whichnade[pid] == index) then
		whichnade[pid] = nil;
	end
end

-- Bypass default grenade detonate behavior
-- TODO: is this the best way of doing things?
function mod.detonate_grenade(index)
	return remove_grenade(index);
end

local function close_to_0(num)
	return math.abs(num) < 0.005;
end

function mod.after.tick()
	for pid in piditer(PID_BROADCAST) do
		if (is_alive(pid) and whichnade[pid] ~= nil) then
			local vel = get_grenade_velocity(whichnade[pid]);
			if (close_to_0(vel.x) and close_to_0(vel.y) and close_to_0(vel.z)) then
				-- TODO: add slowdown when nade stops tracking here?
				whichnade[pid] = nil;
				goto continue;
			end

			local pos = get_grenade_position(whichnade[pid]);
			pos.z = pos.z - 2.251;
			if (bit.band(get_inputs(pid), 32) == 32) then
				pos.z = pos.z + 0.9;
			end
			set_position(pid, pos);
		end
		::continue::
	end
end

return mod;
