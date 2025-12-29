-- command_exec.lua -- Execute arbitrary lua

local cmd = {name="exec", caps="exec"};
function cmd.func(pid, argv, msg)
	assert(loadstring(string.sub(msg, 6, -1)))()
end
register_command(cmd);
