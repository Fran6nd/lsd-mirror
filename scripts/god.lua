-- god.lua -- Prevent any change in HP (except from restock and respawn)
local mod = init_mod();
local god = pid_joined_table(false);

local cmd = {name="god", caps="god", desc="Prevent your HP from being changed."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	god[pid] = not god[pid];
end
register_command(cmd);

function mod.set_hp(pid, hp)
	if (not god[pid]) then
		return next_call("set_hp", mod.set_hp)(pid, hp);
	end
end

function mod.set_hp_directional(pid, hp, pos)
	if (not god[pid]) then
		return next_call("set_hp", mod.set_hp)(pid, hp, pos);
	end
end

return mod;
