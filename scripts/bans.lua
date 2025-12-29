-- bans.lua -- Ban IP address ranges retrieved from an sqlite3 database
local mod = {};
local sql = require "lsqlite3";
local db;
local stmt = {};

local function ban(startaddr, endaddr, bantime, expires, name, comment, bannedby)
	if (endaddr >= 2130706432 and startaddr <= 2147483647) then
		-- You can't ban a loopback address. . .
		return 1;
	end

	-- Shove in database
	stmt.ban:bind_values(startaddr, endaddr, bantime, expires, name, comment, bannedby);
	stmt.ban:step();
	stmt.ban:reset();
	-- TODO: detect updates to database and kick if someone found there; may need inotify, checking often enough or ipc of some sorts

	-- Look for players that should be banned and then ban them
	for i in piditer(PID_BROADCAST) do
		if (is_connected(i)) then
			local addr = get_ipaddr(i);
			if(startaddr <= addr and endaddr >= addr and addr ~= 2130706433) then
				-- TODO: func for consistent logging identifier?
				-- TODO: banned for how long?
				log("Banned %s (#%u)", get_name(i), i);
				send_chat(PID_BROADCAST, get_name(i).." was banned", 2, 0);
				disconnect(i, 1);
			end
		end
	end
end

local function rollstep(stepstmt)
	while (true) do
		code = stepstmt:step();
		-- TODO: can this ever return sql.row?
		if (code == sql.DONE) then
			break;
		end
		if (code ~= sql.ROW) then
			local rollcode = sql.BUSY;
			while (rollcode == sql.BUSY) do
				-- TODO: sqlite may choose to rollback on its own and then throw an error when i try again
				rollcode = db:exec("ROLLBACK;");
			end
			if (rollcode ~= sql.OK) then
				error("db:exec-STEP-ROLLBACK: "..rollcode);
			end
			error("stepstmt:step: "..code);
		end
	end
end

local function moveban(id, add, rm)
	local code;

	code = db:exec("BEGIN;");
	if (code ~= sql.OK) then
		-- TODO: bind_values fail and such make BEGIN unhappy -- never does ROLLBACK
		error("db:exec-BEGIN: "..code);
	end

	add:reset();
	add:bind_values(id);
	rollstep(add);

	rm:reset();
	rm:bind_values(id);
	rollstep(rm);

	code = db:exec("COMMIT;");
	if (code ~= sql.OK) then
		local rollcode = sql.BUSY;
		while (rollcode == sql.BUSY) do
			rollcode = db:exec("ROLLBACK;");
		end
		if (rollcode ~= sql.OK) then
			error("db:exec-COMMIT-ROLLBACK: "..rollcode);
		end
		error("db:exec-COMMIT: "..code);
	end
end

local function is_banned(name, addr)
	local banned;

	-- TODO: queue up os.time calls into one? (maybe do something similar with get_time()
	stmt.sel:bind_values(os.time(), addr, addr);
	banned = stmt.sel:step() == sql.ROW;
	stmt.sel:reset();

	return banned;
end

local function check_banneds(msg)
	for i in piditer(PID_BROADCAST) do
		if (is_banned(get_name(i), get_ipaddr(i))) then
			--send_chat(32, ":3 <"..i..">", 2, 0);
			log(msg.." %s (#%u)", get_name(i), i);
			send_chat(PID_BROADCAST, get_name(i).." was banned", 2, 0);
			disconnect(i, 1);
		end
	end
end

local function send_query(vals, pid)
	-- TODO: should comment be COLLATE NOCASE and greppable?
	send_chat(pid, string.format("#%i: <%s> banned on %s by %s: %s", vals[1], vals[6], os.date("!%Y-%m-%dT%H:%M:%SZ", vals[4]), vals[8], vals[7]), 2, 0);
end

local function query_addr(statement, startaddr, endaddr, pid)
	local vals;

	statement:bind_values(startaddr, endaddr);
	for x in statement:rows() do
		send_query(x, pid);
	end

	return vals;
end

local function query_name(statement, name, pid)
	local vals;

	statement:bind_values(name);
	for x in statement:rows() do
		send_query(x, pid);
	end

	return vals;
end

function mod.on_any_connect(pid)
	-- TODO: how would i let things hook/override specifically this version of the func?
	if (is_banned(get_name(pid), get_ipaddr(pid))) then
		-- TODO: pretty-print the ipaddr
		-- TODO: which port
		log("%s:%u (#%u) attempted to connect but is banned", get_ipaddr(pid), 42069, pid);
		disconnect_now(pid, 1);
		return;
	end
	next_call("on_any_connect", mod.on_any_connect)(pid);
end

local function verifystmt(name, code)
	if (stmt[name] == nil) then
		-- Throwing an error causes unregister to be called
		error("db:prepare-stmt-"..name..": " .. code);
	end
end

local function createstmt(name, stmtsql)
	local code;
	stmt[name], code = db:prepare(stmtsql);
	verifystmt(name, code);
end

function mod.on_load()
	local code, msg;
	db, code, msg = sql.open("rw/bans.db");

	if (db == nil) then
		error("sql.open: " .. msg);
	end

	db:busy_timeout(100);

	db:exec[[
		PRAGMA journal_mode = WAL;
		PRAGMA temp_store = memory;
		CREATE TABLE IF NOT EXISTS BanRanges(id INTEGER PRIMARY KEY AUTOINCREMENT, startaddr INTEGER, endaddr INTEGER, bantime INTEGER, expires INTEGER, name TEXT COLLATE NOCASE, comment TEXT, bannedby TEXT COLLATE NOCASE);
		CREATE TABLE IF NOT EXISTS ArchivedBans(id INTEGER PRIMARY KEY, startaddr INTEGER, endaddr INTEGER, bantime INTEGER, expires INTEGER, name TEXT COLLATE NOCASE, comment TEXT, bannedby TEXT COLLATE NOCASE);
	]];

	-- TODO: add pledge(2)-style flags col -- ban, mute, purgatory, etc.
	createstmt("sel",          "SELECT name, comment FROM BanRanges WHERE expires > ? AND startaddr <= ? AND endaddr >= ?;");
	createstmt("queryaddr",    "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby FROM BanRanges WHERE endaddr >= ? AND startaddr <= ? ORDER BY bantime;");
	createstmt("queryname",    "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby FROM BanRanges WHERE name = ? ORDER BY bantime;");
	createstmt("queryaddrarc", "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby FROM ArchivedBans WHERE endaddr >= ? AND startaddr <= ? ORDER BY bantime;");
	createstmt("querynamearc", "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby FROM ArchivedBans WHERE name = ? ORDER BY bantime;");
	createstmt("ban",          "INSERT INTO BanRanges VALUES(NULL, ?, ?, ?, ?, ?, ?, ?);");
	createstmt("addarchive",   "INSERT INTO ArchivedBans SELECT * FROM BanRanges WHERE id = ?;");
	createstmt("rmarchive",    "DELETE FROM BanRanges WHERE id = ?;");
	createstmt("addunarchive",   "INSERT INTO BanRanges SELECT * FROM ArchivedBans WHERE id = ?;");
	createstmt("rmunarchive",    "DELETE FROM ArchivedBans WHERE id = ?;");

	-- TODO: you can find a better message than "Found ban for"
	check_banneds("Found ban for");
end

function mod.on_unload()
	for _,y in pairs(stmt) do
		y:finalize();
	end

	if (db ~= nil) then
		db:close();
	end
end

local cmd = {name="ban", caps="ban"};
function cmd.func(pid, argv)
	local now = os.time();
	--ban(0, 1024, now, now+60, "jeff", "dirty haxor. . .", "notaburner's leaked password");
	if (ban(2130706433, 2130706433, now, now+60, "not a burner", "he's ugly", "a mouse") == 1) then
		send_chat(pid, "You can't ban a loopback address!", 2, 0);
		return;
	end
	-- TODO: make from arg optional in send_chat
	-- Hopefully this plays well with self-bans. . .
	send_chat(pid, "Ban ID: #"..db:last_insert_rowid(), 2, 0);
end
register_command(cmd);

-- TODO: get rid of commands and most traces on unreg
local cmd = {name="unban", caps="ban"};
function cmd.func(pid, argv)
	moveban(argv[1], stmt.addarchive, stmt.rmarchive);
end
register_command(cmd);

local cmd = {name="reban", caps="ban"};
function cmd.func(pid, argv)
	moveban(argv[1], stmt.addunarchive, stmt.rmunarchive);
	check_banneds("Banned");
end
register_command(cmd);

-- TODO: CIDR notation?
local cmd = {name="queryban", caps="ban"};
function cmd.func(pid, argv)
	local vals;

	if (#argv == 1) then
		vals = query_name(stmt.queryname, argv[1], pid);
	elseif (#argv == 2) then
		vals = query_addr(stmt.queryaddr, argv[1], argv[2], pid);
	else
		send_chat(pid, "Give it a name or a range.", 2, 0);
	end
end
register_command(cmd);

local cmd = {name="queryoldban", caps="ban"};
function cmd.func(pid, argv)
	local vals;

	if (#argv == 1) then
		vals = query_name(stmt.querynamearc, argv[1], pid);
	elseif (#argv == 2) then
		vals = query_addr(stmt.queryaddrarc, argv[1], argv[2], pid);
	else
		send_chat(pid, "Give it a name or a range.", 2, 0);
	end
end
register_command(cmd);

return mod;
