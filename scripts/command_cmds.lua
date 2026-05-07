-- command_cmds.lua -- List available commands
-- TODO: /apropos
local mod = init_mod();

getcfg("cmds_pagesize", 5);

-- TODO: should "Unknown command" from commands.lua be merged with this?
local unknown_cmd_msg = {
	en="Command not found."
};

local help_cmds_msg = {
	en="Were you looking for /commands?"
};

local function sort_cmds(x, y)
	return (x.name[1] or x.name) < (y.name[1] or y.name);
end

local function print_cmd(cmd)
	local name = cmd.name[1] or cmd.name;
	local description = cmd.desc;
	local invocation = cmd.usage;

	if (invocation) then
		name = name .. " " .. invocation;
	end

	if (description) then
		name = name .. " -- " .. description;
	end

	return name;
end

function mod.impl.can_see_command(pid, cmd)
	return not is_fakepid(pid) or cmd.fakepid;
end

local cmd = {name={"cmds", "commands"}, fakepid=true, usage="[page]", desc="Print an alphabetically ordered list of all commands."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local sorted = {};

	if (argv[1]) then
		local start = 1+tonumber(argv[1]-1)*cmds_pagesize;

		for key, val in pairs(commands) do
			if (can_see_command(pid, val) and (val.name[1] or val.name) == key) then
				table.insert(sorted, val);
			end
		end

		table.sort(sorted, sort_cmds);

		if (start < 1) then
			for _, val in ipairs(sorted) do
				server_msg(pid, print_cmd(val));
			end
			return;
		end

		for i=start,start+cmds_pagesize-1 do
			if (i > #sorted) then
				break;
			end
			server_msg(pid, print_cmd(sorted[i]));
		end
		return;
	end

	for key, val in pairs(commands) do
		if (can_see_command(pid, val) and (val.name[1] or val.name) == key) then
			table.insert(sorted, key);
		end
	end

	table.sort(sorted);

	local line = "";
	local pfx = "";

	-- Limit line length to 80-ish chars
	for i,cmd in ipairs(sorted) do
		line = line..pfx..cmd;
		pfx = ", ";

		if (#line >= 75) then
			if (i ~= #sorted) then
				line = line..",";
			end

			server_msg(pid, line);

			line = "";
			pfx = "";
		end
	end

	server_msg(pid, line);
end
register_command(cmd);

-- TODO: should /help display usage for unseen commands? their existence can be confirmed by just attempting to run it, but /help denies it
local cmd = {name={"help", "man"}, fakepid=true, usage="command", desc="Display the invocation and description of a command."};
function cmd.func(pid, argv)
	local val = commands.help;

	if (#argv == 1) then
		val = commands[argv[1]];
	end

	-- TODO: show aliases
	if (val ~= nil and can_see_command(pid, val)) then
		server_msg(pid, print_cmd(val));
	else
		l10n_send_chat(pid, unknown_cmd_msg);
	end

	if (#argv == 0) then
		l10n_send_chat(pid, help_cmds_msg);
	end
end
register_command(cmd);

return mod;
