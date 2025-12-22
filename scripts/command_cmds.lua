-- command_cmds.lua -- List available commands
-- TODO: /apropos

local function sort_cmds(x, y)
	return (x.name[1] or x.name) < (y.name[1] or y.name);
end

local cmd = {name={"cmds", "commands"}};
function cmd.func(pid, argv)
	local sorted = {};

	for key, val in pairs(commands) do
		if ((val.name[1] or val.name) == key) then
			table.insert(sorted, val);
		end
	end

	table.sort(sorted, sort_cmds);

	for _, val in ipairs(sorted) do
		-- print(val);
		-- for x, y in pairs(val) do
		-- 	print(x, y);
		-- end

		send_chat(pid, (val.name[1] or val.name), 2, 0);
	end
end
register_command(cmd);
