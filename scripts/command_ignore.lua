-- command_ignore.lua -- Tranquility
local mod = init_mod();
local silents = pid_connected_table(function() return pid_connected_table(nil) end);

-- TODO: make the desc clear that it only silences for you, not for all
local cmd = {name="ignore", usage="player", desc="Silence a player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local player = get_arg_pid("player", pid, cmd, argv[1]);
	silents[pid][player] = not silents[pid][player];
end
register_command(cmd);

-- TODO: port mute to use send_chat?
function mod.send_chat(pid, msg, type, from)
	if (type == 2 or from < 0 or from >= MAX_PLAYERS) then
		next_call("send_chat", mod.send_chat)(pid, msg, type, from);
		return;
	end

	for i in piditer(pid) do
		if (silents[i] == nil or not silents[i][from]) then
			next_call("send_chat", mod.send_chat)(i, msg, type, from);
		end
	end
end

return mod;
