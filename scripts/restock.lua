-- restock.lua -- Refill ammo and things at a tent
local mod = init_mod();
local timeout = pid_spawn_table(0);

function mod.after.tick()
	local now = get_time();
	local tents = get_tentloc();

	for i in piditer(PID_BROADCAST) do
	if (
		is_alive(i) and
		tent_in_range(get_position(i), tents[get_team(i)]) and
		now >= timeout[i]
	) then
		timeout[i] = now + 8;
		restock(i);
	end
	end
end

return mod;
