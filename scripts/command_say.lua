-- command_say.lua -- Tell the whole server about your wildest fantasies

local cmd = {name="say", caps="say", fakepid=true, usage="msg", desc="Send a message to all players, displayed as if the server sent it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	send_chat(PID_BROADCAST, string.sub(msg, 5), 2, 0);
end
register_command(cmd);

-- TODO: how to ignore fakepids??? especially since they can just reload tab rather easily on websock con
local cmd = {name="chat", fakepid=true, usage="msg", desc="Send a message to all players, as if you sent it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	if (is_fakepid(pid)) then
		send_chat(PID_BROADCAST, string.format("%s: %s", get_name(pid), string.sub(msg, 6)), 0, pid);
	else
		send_chat(PID_BROADCAST, string.sub(msg, 6), 0, pid);
	end
end
register_command(cmd);

return {};
