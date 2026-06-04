-- irc_console.lua -- Run commands from IRC
-- TODO: user COLLATE NOCASE is prone to bugs, get the
-- canonical version from the database before using it
local ldb = require "lib_db";
local buffer = require("string.buffer");
local mod = init_mod();
local sock;
local getname, setnick, rmnick;

-- irc_console_db used to map auth names to irc nicks
getcfg("irc_console_db", "rw/auth.db");
getcfg("irc_console_addr", "127.0.0.1");
getcfg("irc_console_port", 6667);
getcfg("irc_console_nick", "server");
getcfg("irc_console_chan", "#console");
getcfg("irc_console_pfx", "server!");

local no_setnick_msg = {
	en="Unable to set nick. Make sure the user exists and the nick isn't already in use."
};

local no_rmnick_msg = {
	en="Unable to remove nick. Make sure the user exists and has a linked nickname."
};

-- Set by RPL_ISUPPORT
local strip_pfx = ".*";

local nicktopid = {};
local pidtonick = {};
local usertopid = {};
local pidtouser = {};

local function finish_arg(argv, argc, str)
	if     (argc == 0 and argv.src == nil and argv.tags == nil and string.sub(str, 1, 1) == '@') then
		argv.tags = string.sub(str, 2);
	elseif (argc == 0 and argv.src == nil and string.sub(str, 1, 1) == ':') then
		argv.src = string.sub(str, 2);
	else
		if (argc == 0) then
			str = string.upper(str);
		end

		argv[argc] = str;
		argc = argc + 1;
	end

	return argc;
end

local function split_to_table(msg)
	local buf = buffer.new(#msg);
	local squish = false;
	local argv = {};
	local argc = 0;

	for i=1,#msg do
		local chr = string.sub(msg, i, i);

		if (chr ~= ' ') then
			if (argc > 0 and squish and chr == ':') then
				argv[argc] = string.sub(msg, i+1);
				return argv;
			end

			buf:put(chr);
			squish = false;
		elseif (not squish) then
			argc = finish_arg(argv, argc, buf:get());
			squish = true;
		end
	end

	if (#buf) then
		argc = finish_arg(argv, argc, buf:get());
	end

	return argv;
end

local function cmd_tostr(cmd)
	local str = cmd[0];

	for i=1,#cmd do
		if (i ~= #cmd) then
			str = str.." "..cmd[i];
		else
			str = str.." :"..cmd[i];
		end
	end

	return str.."\r\n";
end

local function run_cmd(pid, line)
	log("irc_console: %s (#%u): /%s", get_name(pid), pid, line);
	handle_command(pid, line, true);
end

local function chrpatesc(str)
	return string.gsub(str, "[%%%[%]^]", "%%%0");
end

local function try_add_nick(nick)
	if (nick) then
		local user = getname(nick);

		if (user) then
			local user = user[1];
			local pid = new_fakepid();
			on_fakepid_connect(pid);

			nicktopid[nick] = pid;
			pidtonick[pid] = nick;
			usertopid[user] = pid;
			pidtouser[pid] = user;

			auth_forcelogin(pid, user);
			log("irc_console: got nick: %s -> %s", nick, user);
		else
			nicktopid[nick] = false;

			log("irc_console: got unlinked nick: %s", nick);
		end
	end
end

local function try_rm_nick(nick, nodisconnect)
	if (nick and nicktopid[nick] ~= nil) then
		local pid = nicktopid[nick];

		nicktopid[nick] = nil;
		if (nodisconnect) then
			nicktopid[nick] = false;
		end

		if (pid) then
			free_fakepid(pid);
			pidtonick[pid] = nil;
			usertopid[pidtouser[pid]] = nil;
			pidtouser[pid] = nil;
		end

		if (nodisconnect) then
			log("irc_console: unlinked nick: %s", nick);
		else
			log("irc_console: rm'd nick: %s", nick);
		end
	end
end

local function nick_from_src(src)
	return string.match(src, "^(.-)[!@]");
end

local function on_line(sock, pid, line)
	line = string.gsub(line, "\r$", "", 1);
	--log("%s", line);

	local cmd = split_to_table(line);
	--log("%s", fmtval(cmd));

	if (cmd[0] == "PING") then
		cmd[0] = "PONG";

		sock_send_broadcast(sock, cmd_tostr(cmd));
	elseif (cmd[0] == "PRIVMSG") then
		local nick = nick_from_src(cmd.src);

		-- TODO: should I document that the prefix is a pattern?
		local nopfx = string.gsub(cmd[2], "^"..irc_console_pfx, "", 1);

		if (nopfx ~= cmd[2] and nicktopid[nick]) then
			run_cmd(nicktopid[nick], nopfx);
		end
	elseif (cmd[0] == "JOIN") then
		try_add_nick(nick_from_src(cmd.src));
	elseif (cmd[0] == "PART" or cmd[0] == "QUIT") then
		try_rm_nick(nick_from_src(cmd.src));
	elseif (cmd[0] == "005") then -- RPL_ISUPPORT
		for i=2,#cmd-1 do
			local pfx = string.match(cmd[i], "^PREFIX=%(.-%)(.*)");
			if (pfx ~= nil) then
				strip_pfx = "[^"..chrpatesc(pfx).."].*";
			end
		end
	elseif (cmd[0] == "353") then -- RPL_NAMREPLY
		for nick in string.gmatch(cmd[4], "[^ ]+") do
			try_add_nick(string.match(nick, strip_pfx));
		end
	end
end

local function on_connect()
	return 1;
end

local function null()
end

function mod.on_load()
	db = ldb.open(irc_console_db);

	-- 0 is the version here
	local tbl = "IrcConsoleNameMap0";

	ldb.init_schema(db, [[
CREATE TABLE IF NOT EXISTS ]]..tbl..[[(
	nick TEXT UNIQUE,
	name TEXT COLLATE NOCASE UNIQUE,
	PRIMARY KEY(nick, name)
) WITHOUT ROWID;
	]]);

	-- TODO: probably cram these in a BEGIN..COMMIT; block?
	getname = ldb.prepare_1ret(db, "getname", "SELECT name FROM "..tbl.." WHERE nick = ?;");
	setnick = ldb.prepare_0ret(db, "setnick", "INSERT INTO "..tbl.." VALUES(?, ?) ON CONFLICT(name) DO UPDATE SET nick = ?;");
	rmnick  = ldb.prepare_0ret(db, "rmnick", "DELETE FROM "..tbl.." WHERE name = ?;");

	sock = nil;
	sock = sock_new_tcp_client(irc_console_addr, irc_console_port, {on_line=on_line, on_connect=on_connect, on_disconnect=null});
	sock_send_broadcast(sock, "NICK "..irc_console_nick.."\r\nUSER "..irc_console_nick.." 0 * "..irc_console_nick.."\r\nJOIN "..irc_console_chan.."\r\n");
end

function mod.on_unload()
	for pid, nick in pairs(pidtonick) do
		try_rm_nick(nick);
	end

	ldb.close(db);
	if (sock) then
		sock_close(sock);
	end
end

function mod.late.log(fmt, ...)
	for pid,con in pairs(sock.cons) do
		local msg = string.format(fmt, ...);

		for line in string.gmatch(msg, "[^\n]+") do
			sock_send_broadcast(sock, cmd_tostr{[0]="PRIVMSG", irc_console_chan, line});
		end
	end
	mod.late.next.log(fmt, ...);
end

function mod.late.player_msg(msg, type, from)
	sock_send_broadcast(sock,  cmd_tostr{[0]="PRIVMSG", irc_console_chan, string.format("(%s) %s: %s\n", type == 0 and "Global" or "Team", get_name(from), msg)});
	mod.late.next.player_msg(msg, type, from);
end

function mod.late.server_msg(pid, msg)
	if (pid == PID_BROADCAST or pidtonick[pid]) then
		for line in string.gmatch(msg, "[^\n]+") do
			sock_send_broadcast(sock, cmd_tostr{[0]="PRIVMSG", irc_console_chan, line});
		end

		if (pid ~= PID_BROADCAST) then
			return;
		end
	end

	mod.late.next.server_msg(pid, msg);
end

function mod.late.get_name(pid)
	if (pidtouser[pid]) then
		return "@"..pidtouser[pid];
	end

	return mod.late.next.get_name(pid);
end

-- TODO: don't require reconnect. . !
local cmd = {name="ircnickset", caps="ircnick", fakepid=true, usage="user nick", desc="Link an auth account to an IRC nick."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2);

	local user = argv[1];
	local nick = argv[2];

	if (setnick(nick, user, nick) ~= 1) then
		l10n_send_chat(pid, no_setnick_msg);
		return;
	end

	if (usertopid[user]) then
		try_rm_nick(pidtonick[usertopid[user]], true);
	end

	if (nicktopid[nick] == false) then
		try_add_nick(nick);
	end
end
register_command(cmd, mod);

local cmd = {name="ircnickrm", caps="ircnick", fakepid=true, usage="user", desc="Unlink an auth account from IRC."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	local user = argv[1];

	if (rmnick(user) ~= 1) then
		l10n_send_chat(pid, no_rmnick_msg);
		return;
	end

	if (usertopid[user]) then
		try_rm_nick(pidtonick[usertopid[user]], true);
	end
end
register_command(cmd, mod);

return mod;
