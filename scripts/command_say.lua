-- command_say.lua -- Tell the whole server about your wildest fantasies

local cmd = {name="say", caps="say", fakepid=true, usage="msg", desc="Send a message to all players, displayed as if the server sent it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	send_chat(PID_BROADCAST, string.sub(msg, 5), 2, 0);
end
register_command(cmd);

return {};
