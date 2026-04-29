-- dbljump.lua -- Jump once in mid-air with the sneak key
local mod = init_mod();
local dbljumps = pid_spawn_table(dbljump_jumps);

getcfg("dbljump_jumps", 1);

function mod.on_move_input(pid, bitmask)
	local oldinp = get_inputs(pid);

	mod.next.on_move_input(pid, bitmask);

	-- TODO: might like to be able to just pass it through to the underlying function instead of calling set_jump. . .
	-- TODO: should it check for is_airborne first or not?
	if (dbljumps[pid] > 0 and is_airborne(pid) and bit.band(oldinp, 64) == 0 and bit.band(bitmask, 64) == 64) then
		dbljumps[pid] = dbljumps[pid] - 1;
		set_jump(pid);
	end
end

function mod.after.tick()
	for i in piditer(PID_BROADCAST) do
		if (not is_airborne(i)) then
			dbljumps[i] = dbljump_jumps;
		end
	end
end

return mod;
