-- command_say.lua -- Tell the whole server about your wildest fantasies

local cmd = {name="say", caps="say", fakepid=true, usage="msg", desc="Send a message to all players, displayed as if the server sent it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	server_msg(PID_BROADCAST, string.sub(msg, 5));
end
register_command(cmd, mod);

-- TODO: how to ignore fakepids??? especially since they can just reload tab rather easily on websock con
local cmd = {name="chat", fakepid=true, usage="msg", desc="Send a message to all players, as if you sent it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	-- TODO: hook player_msg in the fakepid impl's and redirect to server_msg there?
	if (is_fakepid(pid)) then
		server_msg(PID_BROADCAST, string.format("%s: %s", get_name(pid), string.sub(msg, 6)));
	else
		player_msg(string.sub(msg, 6), 0, pid);
	end
end
register_command(cmd, mod);

return {};
