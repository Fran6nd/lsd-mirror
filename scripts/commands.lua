-- commands.lua -- Handles chat messages beginning with a / as commands
local buffer = require("string.buffer");
local bit = require("bit");
local mod = init_mod();
-- TODO: do we really want to clear all the commands on load?
commands = {};

-- Set to false by default by that pyspades binding
getcfg("commands_hook_chat", true);
getcfg("commands_register_hook", nil);

function register_command(cmd, mod)
	if (type(cmd.name) == "table") then
		for _, x in ipairs(cmd.name) do
			commands[x] = cmd;
		end
	else
		commands[cmd.name] = cmd;
	end

	if (mod) then
		mod.commands = mod.commands or {};
		mod.commands[cmd] = true;
	end

	if (commands_register_hook ~= nil) then
		commands_register_hook(cmd);
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

		-- TODO: mandate joined? alive? other things?
		if (is_connected(found)) then
			return found
		end
		-- pid not connected
		return -2;
	end

	-- TODO: piditer for joined??
	for i in piditer(PID_BROADCAST) do
		if (is_joined(i) and string.find(string.lower(get_name(i)), string.lower(str), 1, true)) then
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
function cmd_exit()
	error(stexec);
end

function cmd_assert(pid, cmd, condition)
	if (not condition) then
		send_usage(pid, cmd);
		cmd_exit();
	end
end

takenfakepid = {};
function mod.on_load()
	takenfakepid = {};
	for i=0,MAX_PLAYERS-1 do
		takenfakepid[i] = true;
	end
end

function mod.on_unload()
	for i, _ in pairs(takenfakepid) do
		if (i >= MAX_PLAYERS) then
			free_fakepid(i);
		end
	end
end

function mod.after.unregister(module)
	if (module.commands == nil) then
		return;
	end

	for name, cmd in pairs(commands) do
		if (module.commands[cmd] ~= nil) then
			commands[name] = nil;
		end
	end
end

-- TODO: just use on_successful_connect?
function mod.impl.on_fakepid_connect(pid)
end

function new_fakepid()
	local pid = #takenfakepid+1;
	takenfakepid[pid] = true;
	return pid;
end

-- TODO: make pid_tables hook into this instead of the other way around
function free_fakepid(pid)
	if (clear_fakepid_table) then
		clear_fakepid_table(pid);
	end
	takenfakepid[pid] = nil;
end

function is_fakepid(pid)
	return pid >= MAX_PLAYERS and pid < 0x20000000;
end

function get_arg_str(argname, pid, cmd, arg)
	if (arg == nil) then
		send_usage(pid, cmd);
		cmd_exit();
	end
	return arg;
end

-- TODO: should l10n format decimal values in a language-specific manner?
local cidr_msg = {
	en="%(arg) should be a IPv4 address, optionally given in CIDR notation."
};

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

function get_arg_cidr(argname, pid, cmd, arg)
	if (arg == nil) then
		send_usage(pid, cmd);
		cmd_exit();
	end

	local a1, a2, a3, a4, range = string.match(arg, "^(%d+)%.(%d+)%.(%d+)%.(%d+)/?(%d*)$");

	a1 = tonumber(a1);
	a2 = tonumber(a2);
	a3 = tonumber(a3);
	a4 = tonumber(a4);
	range = tonumber(range);

	if (range == nil) then
		range = 32;
	end

	-- None of these can be negative without failing the string.match()
	if (
		a1 == nil or
		a1 > 255 or
		a2 > 255 or
		a3 > 255 or
		a4 > 255 or
		range > 32
	) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, cidr_msg, {arg=argname});
		cmd_exit();
	end

	local addr = bit.bor(bit.lshift(a1, 24), bit.lshift(a2, 16), bit.lshift(a3, 8), a4);
	local mask;

	if (range == 0) then
		mask = 0xffffffff;
	else
		mask = bit.lshift(1, 32 - range) - 1;
	end

	local starta, enda = bit.band(addr, bit.bnot(mask)), bit.bor(addr, mask);

	if (starta < 0) then
		starta = 2^32 + starta;
	end

	if (enda < 0) then
		enda = 2^32 + enda;
	end

	return starta, enda;
end

function get_arg_time(argname, pid, cmd, arg)
	local umap = {s=1, min=60, h=60*60, d=60*60*24, w=60*60*24*7, month=60*60*24*30, y=60*60*24*365};
	local num, unit = string.match(arg, "^(%d+)(%a*)$");
	num = tonumber(num);
	if (num == nil) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, delta_msg, {arg=argname});
		cmd_exit();
	end
	-- TODO: It should probably be finite, right?
	if (not (num > -math.huge and num < math.huge)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, finite_msg, {arg=argname});
		cmd_exit();
	end

	if (unit == "") then
		-- TODO: should it default to seconds or something else?
		return num;
	elseif (umap[string.lower(unit)]) then
		return num * umap[unit];
	end

	send_usage(pid, cmd);
	l10n_send_chat(pid, unit_msg, {arg=argname});
	cmd_exit();
end

function get_arg_num_nonfinite(argname, pid, cmd, arg)
	local num = tonumber(arg);
	if (num == nil) then
		send_usage(pid, cmd);
		cmd_exit();
	end
	return num;
end

function get_arg_num_range_opt(argname, pid, cmd, arg, start, endval)
	if (arg == nil) then
		return nil;
	end
	local num = tonumber(arg);
	if (num == nil or not (num >= start and num <= endval)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, range_msg, {arg=argname, min=start, max=endval});
		cmd_exit();
	end
	return num;
end

function get_arg_num_range(argname, pid, cmd, arg, start, endval)
	local num = get_arg_num_range_opt(argname, pid, cmd, arg, start, endval);
	if (num == nil) then
		send_usage(pid, cmd);
		cmd_exit();
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
		cmd_exit();
	end
	return num;
end

function get_arg_num_finite(argname, pid, cmd, arg)
	local num = get_arg_num_finite_opt(argname, pid, cmd, arg);
	if (num == nil) then
		send_usage(pid, cmd);
		cmd_exit();
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
		cmd_exit();
	end
	return plr;
end

function get_arg_pid(argname, pid, cmd, arg)
	local plr = get_arg_pid_opt(argname, pid, cmd, arg);
	if (plr == nil) then
		send_usage(pid, cmd);
		cmd_exit();
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

-- fun read, my adaptation is probably not as good: https://nullprogram.com/blog/2021/12/04/
local quottable = {[0]={}, [1]={}};
for i=0,255 do
	local chr = string.char(i);
	quottable[0][i] = chr;
	quottable[1][i] = chr;
end

-- isspace(3), C locale
quottable[0][0x09] = nil;
quottable[0][0x0a] = nil;
quottable[0][0x0b] = nil;
quottable[0][0x0c] = nil;
quottable[0][0x0d] = nil;
quottable[0][0x20] = nil;

local function unquote_to_table(msg)
	local buf = buffer.new(#msg);
	local state = 0;
	local exclstate = 1;
	local squish = false;
	local argv = {};
	local argc = 0;

	for i=1,#msg do
		local inchr = string.byte(msg, i);
		local outchr = quottable[state][inchr];

		-- 0x22 is '"'
		state = bit.bxor(state, inchr == 0x22 and 1 or 0);
		exclstate = bit.bor(bit.bxor(exclstate, 1), inchr ~= 0x22 and 1 or 0);

		if (outchr) then
			if (exclstate ~= 0) then
				buf:put(outchr);
			end
			squish = false;
		elseif (not squish) then
			argv[argc] = buf:get();
			argc = argc + 1;
			squish = true;
		end
	end

	if (not squish) then
		argv[argc] = buf:get();
		argc = argc + 1;
	end

	return argv;
end

-- TODO: log
-- TODO: /mute
-- TODO: redact certain args?
function handle_command(pid, msg, nolog)
	local i = 0;
	local argv = unquote_to_table(msg);
	local cmd = nil;

	if (argv[0] ~= nil) then
		cmd = commands[string.lower(argv[0])];
	end

	if (not nolog) then
		if (cmd and cmd.sensitive) then
			msg = argv[0].." [REDACTED]";
		end

		server_msg(pid, "> /"..msg);
		log("%s: /%s", get_name(pid), msg);
	end

	if (cmd == nil) then
		l10n_send_chat(pid, unknown_cmd_msg);
		return;
	end

	try_run_command(commands[string.lower(argv[0])], pid, argv, msg);
end

function mod.impl.try_run_command(cmd, pid, argv, msg)
	if (is_fakepid(pid) and not cmd.fakepid) then
		l10n_send_chat(pid, not_in_game_msg);
		return;
	end
	local status, err = pcall(cmd.func, pid, argv, msg);
	if (not status and err ~= stexec) then
		l10n_send_chat(pid, cmd_err_msg);
		error(err, 2);
	end
end

function send_usage(pid, cmd)
	local name = "usage: "..(cmd.name[1] or cmd.name);
	local invocation = cmd.usage;

	if (invocation) then
		name = name .. " " .. invocation;
	end

	server_msg(pid, name);
end

if (commands_hook_chat) then
function mod.early.on_chat(pid, msg, type)
	if (string.sub(msg, 1, 1) == "/") then
		handle_command(pid, string.sub(msg, 2, -1));
		return;
	end

	mod.early.next.on_chat(pid, msg, type);
end
end

return mod;
