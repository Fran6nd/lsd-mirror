-- bans.lua -- Ban IP address ranges retrieved from an sqlite3 database
require "lib_l10n";
local mod = {after={}};
local sql = require "lsqlite3";
local db;
local stmt = {};

-- TODO: badcaps is too easy to confuse with bancaps
getcfg("bans_db", "rw/bans.db");
getcfg("bans_default_badcaps", "ban");

local badcap_exp = {};
local badcap_exp_next = {};
local function rm_badcaps(pid, now)
	local shoulddrop = {};
	badcap_exp_next[pid] = nil;

	for x,y in pairs(badcap_exp[pid]) do
		if (now >= x) then
			sc("drop ts: "..tostring(x));
			for _,cap in ipairs(y) do
				if (shoulddrop[cap] == nil) then
					shoulddrop[cap] = true;
				end
			end
			-- TODO: don't set to nil if there are still unexpired badcaps which have it set
			badcap_exp[pid][x] = nil;
		else
			for _,cap in ipairs(y) do
				shoulddrop[cap] = false;
			end
			if (badcap_exp_next[pid] == nil or x < badcap_exp_next[pid]) then
				badcap_exp_next[pid] = x;
			end
		end
	end

	for x,y in pairs(shoulddrop) do
		if (y) then
			sc("drop "..x);
			drop_cap(pid, x);
		end
	end
end
local function tick_badcaps()
	local now = os.time();
	for i in piditer(PID_BROADCAST) do
		if (badcap_exp_next[i] and now >= badcap_exp_next[i]) then
			sc("tick #"..tostring(i));
			rm_badcaps(i, now);
		end
	end
end
local function setup_badcap_exp(pid, expires)
	if (badcap_exp_next[pid] == nil or expires < badcap_exp_next[pid]) then
		badcap_exp_next[pid] = expires;
		sc("addto #"..tostring(pid).." -- ts: "..tostring(expires));
	end
	if (badcap_exp[pid][expires] == nil) then
		badcap_exp[pid][expires] = {};
	end
end
local function add_badcaps(pid)
	-- TODO: query multiple bans (not just one!) and concatenate them
	stmt.sel:bind_values(os.time(), get_ipaddr(pid), get_ipaddr(pid));
	for row in stmt.sel:rows() do
		setup_badcap_exp(pid, row[4]);
		for x in string.gmatch(row[3], "%S+") do
			grant_cap(pid, "badcap:"..x);
			table.insert(badcap_exp[pid][row[4]], "badcap:"..x);
		end
	end
end

local function ban(startaddr, endaddr, bantime, expires, name, comment, bannedby, flags)
	-- Shove in database
	stmt.ban:bind_values(startaddr, endaddr, bantime, expires, name, comment, bannedby, flags);
	stmt.ban:step();
	stmt.ban:reset();
	-- TODO: detect updates to database and kick if someone found there; may need inotify, checking often enough or ipc of some sorts

	-- Look for players that should be banned and then ban them
	for i in piditer(PID_BROADCAST) do
		if (is_connected(i)) then
			local addr = get_ipaddr(i);
			if(startaddr <= addr and endaddr >= addr and addr ~= 2130706433) then
				setup_badcap_exp(i, expires);
				for x in string.gmatch(flags, "%S+") do
					grant_cap(i, "badcap:"..x);
					table.insert(badcap_exp[i][expires], "badcap:"..x);
				end
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

-- TODO: localize the log
local banned_msg = {
	en="%(name) was banned"
};

local attemptingconnect = false
-- TODO: doesn't this seem a lot like temporary groups? -- should i merge half of this with auth.lua, maybe make tmp_groups.lua and add bans.lua as a thin layer over that?
-- TODO: or maybe make caps.lua and make auth and bans (tmp_caps?) a dependent?
function mod.after.on_cap_grant(pid, cap)
	-- TODO: func for consistent logging identifier?
	-- TODO: banned for how long?
	if (not attemptingconnect and cap == "badcap:ban") then
		log("Banned %s (#%u)", get_name(pid), pid);
		l10n_send_chat(PID_BROADCAST, banned_msg, {name=get_name(pid)});
		disconnect(pid, 1);
	end
end

local function check_banneds(msg)
	for i in piditer(PID_BROADCAST) do
		add_badcaps(i);
	end
end

local query_msg = {
	en="#%(id): <%(name)> banned on %(date) by %(banner): %(comment) (flags: %(flags))"
};

local function send_query(vals, pid)
	-- TODO: should comment be COLLATE NOCASE and greppable?
	l10n_send_chat(pid, query_msg, {id=vals[1], name=vals[6], date=os.date("!%Y-%m-%dT%H:%M:%SZ", vals[4]), banner=vals[8], comment=vals[7], flags=vals[9]});
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
	badcap_exp[pid] = {};
	badcap_exp_next[pid] = nil;
	attemptingconnect = true;
	-- TODO: how would i let things hook/override specifically this version of the func?
	add_badcaps(pid);
	attemptingconnect = false;
	-- TODO: just send the "attempted to connect" message, not also the "Banned" one
	if (has_cap(pid, "badcap:ban")) then
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
	db, code, msg = sql.open(bans_db);

	if (db == nil) then
		error("sql.open: " .. msg);
	end

	db:busy_timeout(100);

	db:exec[[
		PRAGMA journal_mode = WAL;
		PRAGMA temp_store = memory;
		CREATE TABLE IF NOT EXISTS BanRanges(id INTEGER PRIMARY KEY AUTOINCREMENT, startaddr INTEGER, endaddr INTEGER, bantime INTEGER, expires INTEGER, name TEXT COLLATE NOCASE, comment TEXT, bannedby TEXT COLLATE NOCASE, flags TEXT COLLATE NOCASE);
		CREATE TABLE IF NOT EXISTS ArchivedBans(id INTEGER PRIMARY KEY, startaddr INTEGER, endaddr INTEGER, bantime INTEGER, expires INTEGER, name TEXT COLLATE NOCASE, comment TEXT, bannedby TEXT COLLATE NOCASE, flags TEXT COLLATE NOCASE);
	]];

	createstmt("sel",          "SELECT name, comment, flags, expires FROM BanRanges WHERE expires > ? AND startaddr <= ? AND endaddr >= ?;");
	createstmt("queryaddr",    "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby, flags FROM BanRanges WHERE endaddr >= ? AND startaddr <= ? ORDER BY bantime;");
	createstmt("queryname",    "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby, flags FROM BanRanges WHERE name = ? ORDER BY bantime;");
	createstmt("queryaddrarc", "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby, flags FROM ArchivedBans WHERE endaddr >= ? AND startaddr <= ? ORDER BY bantime;");
	createstmt("querynamearc", "SELECT id, startaddr, endaddr, bantime, expires, name, comment, bannedby, flags FROM ArchivedBans WHERE name = ? ORDER BY bantime;");
	createstmt("ban",          "INSERT INTO BanRanges VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?);");
	createstmt("addarchive",   "INSERT INTO ArchivedBans SELECT * FROM BanRanges WHERE id = ?;");
	createstmt("rmarchive",    "DELETE FROM BanRanges WHERE id = ?;");
	createstmt("addunarchive",   "INSERT INTO BanRanges SELECT * FROM ArchivedBans WHERE id = ?;");
	createstmt("rmunarchive",    "DELETE FROM ArchivedBans WHERE id = ?;");


	badcap_exp = {};
	badcap_exp_next = {};
	for i in piditer(PID_BROADCAST) do
		badcap_exp[i] = {};
		-- badcap_exp_next[i] is implicitly nil
	end

	-- TODO: you can find a better message than "Found ban for"
	check_banneds("Found ban for");
end

-- TODO: remove bancaps on unload?
function mod.on_unload()
	for _,y in pairs(stmt) do
		y:finalize();
	end

	if (db ~= nil) then
		db:close();
	end
end

function mod.after.tick()
	tick_badcaps();
end

local ban_id_msg = {
	en="Ban ID: #%(id)"
};

local cmd = {name="ban", caps="ban", usage="player duration comment", desc="Ban a naughty player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 3);
	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	-- TODO: get_arg_time?
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local reason = argv[3];
	local addr = get_ipaddr(banpid);

	local now = os.time();
	-- TODO: link up with auth.lua for banner name determination
	ban(addr, addr, now, now+duration, get_name(banpid), reason, get_name(pid), bans_default_badcaps);
	-- TODO: make from arg optional in send_chat
	-- Hopefully this plays well with self-bans. . .
	l10n_send_chat(pid, ban_id_msg, {id=db:last_insert_rowid()});
end
register_command(cmd);

local cmd = {name="banflags", caps="ban", usage="player duration comment flags...", desc="Give a naughty player some ban flags."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv > 3);
	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	-- TODO: get_arg_time?
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local reason = argv[3];
	local addr = get_ipaddr(banpid);

	local now = os.time();
	-- TODO: link up with auth.lua for banner name determination
	ban(addr, addr, now, now+duration, get_name(banpid), reason, get_name(pid), table.concat(argv, " ", 4));
	-- TODO: make from arg optional in send_chat
	-- Hopefully this plays well with self-bans. . .
	l10n_send_chat(pid, ban_id_msg, {id=db:last_insert_rowid()});
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
-- TODO: completely redo this
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
