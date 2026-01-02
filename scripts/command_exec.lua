-- command_exec.lua -- Execute arbitrary lua

local cmd = {name="exec", caps="exec", usage="lua", desc="Execute arbitrary lua. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	assert(loadstring(string.sub(msg, 6, -1)))()
end
register_command(cmd);
