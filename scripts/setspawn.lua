-- setspawn.lua -- Command that sets your spawn position
local mod = init_mod();
local spawnpos = pid_joined_table(nil);
-- TODO: add defaultdefault cap, make default cap point to that, . . . this seems complicated

local nospawn_msg = {
	en="You'll now spawn at the default position."
};

-- TODO: hide position?
local spawning_msg = {
	en="You'll now spawn at {x=%(x), y=%(y), z=%(z)}."
};

-- TODO: pycapi-style noarg option?
local cmd = {name="setspawn", caps="setspawn", desc="Toggle respawning at your current position."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	if (spawnpos[pid]) then
		spawnpos[pid] = nil;
		l10n_send_chat(pid, nospawn_msg);
	else
		-- TODO: mention somewhere that dead people can use /setspawn
		spawnpos[pid] = get_position(pid);
		l10n_send_chat(pid, spawning_msg, {
			x=string.format("%.2f", spawnpos[pid].x),
			y=string.format("%.2f", spawnpos[pid].y),
			z=string.format("%.2f", spawnpos[pid].z)
		});
	end
end
register_command(cmd, mod);

function mod.on_player_spawn(pid)
	if (spawnpos[pid]) then
		return spawnpos[pid];
	end

	return mod.next.on_player_spawn(pid);
end

return mod;
