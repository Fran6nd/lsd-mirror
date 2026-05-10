-- god.lua -- Prevent any change in HP (except from restock and respawn)
local mod = init_mod();
local god = pid_joined_table(false);

local cmd = {name="god", caps="god", desc="Prevent yourself from being damaged."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	god[pid] = not god[pid];
end
register_command(cmd);

function mod.damage_player(pid, ...)
	if (not god[pid]) then
		return mod.next.damage_player(pid, ...);
	end
end

function mod.damage_player_directional(pid, ...)
	if (not god[pid]) then
		return mod.next.damage_player_directional(pid, ...);
	end
end

return mod;
