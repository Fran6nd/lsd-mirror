-- ffa.lua -- Now we can all be enemies!
local mod = init_mod();

function mod.send_spawn_player(pid, pos, gun, team, name, from)
	for i in piditer(PID_BROADCAST) do
		if (i == from and team ~= SPECTATOR) then
			mod.next.send_spawn_player(i, pos, gun, 0, name, from);
		else
			mod.next.send_spawn_player(i, pos, gun, team, name, from);
		end
	end
end

function mod.on_join(pid, team, gun, name)
	if (team ~= SPECTATOR) then
		team = 1;
	end

	return mod.next.on_join(pid, team, gun, name);
end

function mod.on_switch(pid, team, gun)
	if (team ~= SPECTATOR) then
		team = 1;
	end

	if (team ~= get_next_team(pid) or gun ~= get_next_gun(pid)) then
		return mod.next.on_switch(pid, team, gun);
	end
end

-- Out-of-the-box on_hit doesn't allow hitting players of the same team
function mod.on_hit(pid, type, hitPlayer)
	damage_player_directional(
		hitPlayer,
		get_hit_damage(pid, type),
		get_position(pid),
		type == 4 and 2 or (type == 1 and 1 or 0),
		pid
	);
end

-- TODO: need to be able to hook grenades to override who they can hit

return mod;
