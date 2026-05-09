-- command_exec.lua -- Execute arbitrary lua

local cmd = {name="exec", caps="exec", fakepid=true, usage="lua", desc="Execute arbitrary lua. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	local func, err = loadstring(string.sub(msg, 6));

	if (func == nil) then
		server_msg(pid, "loadstring: "..tostring(err));
		return;
	end

	local status, err = pcall(func);
	if (status == false) then
		server_msg(pid, "pcall: "..tostring(err));
		return;
	end

	if (err ~= nil) then
		-- err being whatever was returned, if applicable
		for line in string.gmatch(tostring(err), "[^\n]+") do
			server_msg(pid, line);
		end
	end
end
register_command(cmd);

return {};
