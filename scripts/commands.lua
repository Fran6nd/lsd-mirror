-- commands.lua -- Handles chat messages beginning with a / as commands
require "lib_l10n";
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

-- TODO: make a version that only operates on joined and a version that only operates on connected
function get_player_by_str(str)
	-- Substring not found
	local found = -1;

	if (str == nil) then
		return nil;
	end

	-- TODO: should /^#.*[^0-9]/ (i.e. not /^#%d+$/) just do a substr match?
	if (string.sub(str, 1, 1) == "#") then
		local found = tonumber(string.sub(str, 2, -1));
		-- TODO: implement validation into is_connected
		-- The not is there since all comparisons against NaN are false.
		if (found == nil or not (found >= 0 and found < MAX_PLAYERS)) then
			-- pid invalid
			return -4;
		end

		if (is_connected(found)) then
			return found
		end
		-- pid not connected
		return -2;
	end

	for i in piditer(PID_BROADCAST) do
		if (string.find(string.lower(get_name(i)), string.lower(str), 1, true)) then
			if (found >= 0) then
				-- Ambiguous
				return -3;
			end
			found = i;
		end
	end

	return found;
end

local stexec = {};
function cmd_assert(pid, cmd, condition)
	if (not condition) then
		send_usage(pid, cmd);
		error(stexec);
	end
end

function is_fakepid(pid)
	return pid >= MAX_PLAYERS and pid < 0x20000000;
end

function get_arg_str(argname, pid, cmd, arg)
	if (arg == nil) then
		send_usage(pid, cmd);
		error(stexec);
	end
	return arg;
end

-- TODO: should l10n format decimal values in a language-specific manner?
local delta_msg = {
	en="%(arg) should be a time delta."
};

local finite_msg = {
	en="%(arg) should be finite."
};

local unit_msg = {
	en="%(arg) should use at most one of the following units: s, min, h, d, w, month, y."
};

local range_msg = {
	en="%(arg) should be between %(min) and %(max)"
};

function get_arg_time(argname, pid, cmd, arg)
	local umap = {s=1, min=60, h=60*60, d=60*60*24, w=60*60*24*7, month=60*60*24*30, y=60*60*24*365};
	local num, unit = string.match(arg, "^(%d+)(%a*)$");
	num = tonumber(num);
	if (num == nil) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, delta_msg, {arg=argname});
		error(stexec);
	end
	-- TODO: It should probably be finite, right?
	if (not (num > -math.huge and num < math.huge)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, finite_msg, {arg=argname});
		error(stexec);
	end

	if (unit == "") then
		-- TODO: should it default to seconds or something else?
		return num;
	elseif (umap[string.lower(unit)]) then
		return num * umap[unit];
	end

	send_usage(pid, cmd);
	l10n_send_chat(pid, unit_msg, {arg=argname});
	error(stexec);
end

function get_arg_num_nonfinite(argname, pid, cmd, arg)
	local num = tonumber(arg);
	if (num == nil) then
		send_usage(pid, cmd);
		error(stexec);
	end
	return num;
end

function get_arg_num_range(argname, pid, cmd, arg, start, endval)
	local num = tonumber(arg);
	if (num == nil) then
		send_usage(pid, cmd);
		error(stexec);
	end
	if (not (num >= start and num <= endval)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, range_msg, {arg=argname, min=start, max=endval});
		error(stexec);
	end
	return num;
end

function get_arg_num_finite_opt(argname, pid, cmd, arg)
	if (arg == nil) then
		return nil;
	end
	local num = tonumber(arg);
	if (num == nil or not (num > -math.huge and num < math.huge)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, finite_msg, {arg=argname});
		error(stexec);
	end
	return num;
end

function get_arg_num_finite(argname, pid, cmd, arg)
	local num = get_arg_num_finite_opt(argname, pid, cmd, arg);
	if (num == nil) then
		send_usage(pid, cmd);
		error(stexec);
	end
	return num;
end

local substr_not_found_msg = {
	en="%(arg): Player not found."
};

local pid_not_connected_msg = {
	en="%(arg): Player not connected."
};

local substr_ambiguous_msg = {
	en="%(arg): Ambiguous player."
};

local pid_invalid_msg = {
	en="%(arg): Invalid player ID."
};

function get_arg_pid_opt(argname, pid, cmd, arg)
	local plr = get_player_by_str(arg);
	if (plr ~= nil and plr < 0) then
		send_usage(pid, cmd);
		if (plr == -1) then
			l10n_send_chat(pid, substr_not_found_msg, {arg=argname});
		elseif (plr == -2) then
			l10n_send_chat(pid, pid_not_connected_msg, {arg=argname});
		elseif (plr == -3) then
			l10n_send_chat(pid, substr_ambiguous_msg, {arg=argname});
		elseif (plr == -4) then
			l10n_send_chat(pid, pid_invalid_msg, {arg=argname});
		end
		error(stexec);
	end
	return plr;
end

function get_arg_pid(argname, pid, cmd, arg)
	local plr = get_arg_pid_opt(argname, pid, cmd, arg);
	if (plr == nil) then
		send_usage(pid, cmd);
		error(stexec);
	end
	return plr;
end

-- TODO: unregister commands somehow
-- local orig_unreg = unregister;
-- function unregister(module)
-- 	orig_unreg(module);
-- end

-- TODO: should this be punctuated?
local unknown_cmd_msg = {
	en="Unknown command"
};

local cmd_err_msg = {
	en="Some error occurred with that command :("
};

local not_in_game_msg = {
	en="This command can only be run while in-game."
};

-- TODO: log
-- TODO: /mute
-- TODO: redact certain args?
function handle_command(pid, msg, nolog)
	local i = 0;
	local argv = {};

	for x in string.gmatch(msg, "%S+") do
		argv[i] = x;
		i = i + 1;
	end

	if (not nolog) then
		send_chat(pid, "> /"..msg, 2, 0);
		log("%s: /%s", get_name(pid), msg);
	end

	if (argv[0] == nil or commands[string.lower(argv[0])] == nil) then
		l10n_send_chat(pid, unknown_cmd_msg);
		return;
	end

	try_run_command(commands[string.lower(argv[0])], pid, argv, msg);
end

function try_run_command(cmd, pid, argv, msg)
	if (is_fakepid(pid) and not cmd.fakepid) then
		l10n_send_chat(pid, not_in_game_msg);
		return;
	end
	local status, err = pcall(cmd.func, pid, argv, msg);
	if (not status and err ~= stexec) then
		l10n_send_chat(pid, cmd_err_msg);
		error(err);
	end
end
server.try_run_command = try_run_command;

function send_usage(pid, cmd)
	local name = "usage: "..(cmd.name[1] or cmd.name);
	local invocation = cmd.usage;

	if (invocation) then
		name = name .. " " .. invocation;
	end

	send_chat(pid, name, 2, 0);
end

function mod.on_chat(pid, msg, type)
	if (string.sub(msg, 1, 1) == "/") then
		handle_command(pid, string.sub(msg, 2, -1));
		return;
	end

	next_call("on_chat", mod.on_chat)(pid, msg, type);
end

return mod;
