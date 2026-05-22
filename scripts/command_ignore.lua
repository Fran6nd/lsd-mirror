-- command_ignore.lua -- Tranquility
local mod = init_mod();
-- Don't you think the name is a bit long?
cmd_ignore_ignored = nil;

function mod.on_load()
	cmd_ignore_ignored = pid_connected_table(function() return pid_connected_table(nil) end);
end

function mod.on_unload()
	cmd_ignore_ignored = nil;
end

-- TODO: make the desc clear that it only silences for you, not for all
local cmd = {name="ignore", usage="player", desc="Silence a player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local player = get_arg_pid("player", pid, cmd, argv[1]);
	cmd_ignore_ignored[pid][player] = not cmd_ignore_ignored[pid][player];
end
register_command(cmd, mod);

-- TODO: port mute to use send_chat?
function mod.send_chat(pid, msg, type, from)
	if (type == 2 or from < 0 or from >= MAX_PLAYERS) then
		mod.next.send_chat(pid, msg, type, from);
		return;
	end

	for i in piditer(pid) do
		if (cmd_ignore_ignored[i] == nil or not cmd_ignore_ignored[i][from]) then
			mod.next.send_chat(i, msg, type, from);
		end
	end
end

return mod;
