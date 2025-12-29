-- commands.lua -- Handles chat messages beginning with a / as commands
local mod = {};
-- TODO: do we really want to clear all the commands on load?
commands = {};

function register_command(cmd)
	if (type(cmd.name) == "table") then
		for _, x in ipairs(cmd.name) do
			commands[x] = cmd;
		end
	else
		commands[cmd.name] = cmd;
	end
end

-- TODO: unregister commands somehow
local orig_unreg = unregister;
function unregister(module)
	orig_unreg(module);
end

-- TODO: log
-- TODO: /mute
-- TODO: redact certain args?
local function handle_command(pid, msg)
	local i = 0;
	local argv = {};

	for x in string.gmatch(msg, "([^%s]+)") do
		argv[i] = x;
		i = i + 1;
	end

	send_chat(pid, "> /"..msg, 2, 0);
	log("%s: /%s", get_name(pid), msg);

	if (commands[string.lower(argv[0])] == nil) then
		send_chat(pid, "Unknown command", 2, 0);
		return;
	end

	try_run_command(commands[string.lower(argv[0])], pid, argv, msg);
end

function try_run_command(cmd, pid, argv, msg)
	local status, err = pcall(cmd.func, pid, argv, msg);
	if (not status) then
		send_chat(pid, "Some error occurred with that command :(", 2, 0);
		error(err);
	end
end
server.try_run_command = try_run_command;

function mod.on_chat(pid, msg, type)
	if (string.sub(msg, 1, 1) == "/") then
		handle_command(pid, string.sub(msg, 2, -1));
		return;
	end

	next_call("on_chat", mod.on_chat)(pid, msg, type);
end

return mod;
