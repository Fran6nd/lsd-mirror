-- bans.lua -- Add (bad)caps to IP address ranges retrieved from an sqlite3 database
local mod = init_mod();
local ldb = require "lib_db";
local db;

getcfg("bans_db",           "rw/bans.db");
getcfg("bans_default_caps", "badcap:ban");
getcfg("bans_server_name",   nil);

-- TODO: localize the log
local banned_msg = {
	en="%(name) was banned"
};

local query_msg = {
	en="#%(id): <%(name)>, %(date) by %(banner): %(comment) (caps: %(caps))"
};

local query_msg_expired = {
	en="#%(id): <%(name)>, expired, %(date) by %(banner): %(comment) (caps: %(caps))"
};

local query_msg_revoked = {
	en="#%(id): <%(name)>, revoked, %(date) by %(banner): %(comment) (caps: %(caps))"
};

local ban_id_msg = {
	en="Ban ID: #%(id)"
};

local ban_id_invalid_msg = {
	en="Invalid ban ID."
};

local attemptingconnect = false;
function mod.after.on_cap_grant(pid, cap)
	if (not attemptingconnect and cap == "badcap:ban") then
		log("Banned %s (#%u)", get_name(pid), pid);
		l10n_send_chat(PID_BROADCAST, banned_msg, {name=get_name(pid)});
		disconnect(pid, 1);
	end
end

local data_ver, sel, ban, revoke, unrevoke, queryaddr, queryname, querycomment;

local bans = pid_connected_table(function() return {}; end);
local function add_ban(pid, id, tbl)
	bans[pid][id] = tbl;
end

local function rm_ban(pid, id, tbl)
	bans[pid][id] = nil;
end

local function sync_bans_with_caps(pid, oldcaps, newcaps)
	for cap,_ in pairs(newcaps) do
		if (oldcaps[cap] == nil) then
			if (is_joined(pid)) then
				log("bans: added %s to %s (#%u)", cap, get_name(pid), pid);
			else
				log("bans: added %s to #%u", cap, pid);
			end
			grant_cap(pid, cap);
		end
	end

	for cap,_ in pairs(oldcaps) do
		if (newcaps[cap] == nil) then
			if (is_joined(pid)) then
				log("bans: dropped %s from %s (#%u)", cap, get_name(pid), pid);
			else
				log("bans: added %s to #%u", cap, pid);
			end
			drop_cap(pid, cap);
		end
	end
end

local function check_bans(pid)
	local oldcaps = {};
	local newcaps = {};

	for id, tbl in pairs(bans[pid]) do
		rm_ban(pid, id);

		for cap in pairs(tbl.caps) do
			oldcaps[cap] = true;
		end
	end

	-- TODO: tell difference between expiration and revoking? (reliably)
	for id, caps, expires in sel(get_ipaddr(pid)) do
		local capstbl = {};

		for cap in string.gmatch(caps, "%S+") do
			capstbl[cap] = true;
			newcaps[cap] = true;
		end

		if (bans[pid][id] == nil) then
			add_ban(pid, id, {caps=capstbl, expires=expires});
		end
	end

	sync_bans_with_caps(pid, oldcaps, newcaps);
end

local function tick_bans(now)
	for i in piditer(PID_BROADCAST) do
		local oldcaps = {};
		local newcaps = {};
		local changed = false;

		for id, tbl in pairs(bans[i]) do
			if (now >= tbl.expires) then
				rm_ban(i, id);

				for cap in pairs(tbl.caps) do
					oldcaps[cap] = true;
				end

				changed = true;
			end
		end

		if (changed) then
			for id, tbl in pairs(bans[i]) do
				for cap in pairs(tbl.caps) do
					oldcaps[cap] = true;
					newcaps[cap] = true;
				end
			end

			sync_bans_with_caps(i, oldcaps, newcaps);
		end
	end
end

function mod.on_load()
	db = ldb.open(bans_db);

	-- 0 is the version here
	local tbl = "Bans0";

	ldb.init_schema(db, [[
CREATE TABLE IF NOT EXISTS ]]..tbl..[[(
	id INTEGER PRIMARY KEY,
	startaddr INTEGER,
	endaddr INTEGER,
	expires INTEGER,
	revoked INTEGER,
	bantime INTEGER,
	playername TEXT COLLATE NOCASE,
	comment TEXT COLLATE NOCASE,
	server TEXT,
	bannedby TEXT COLLATE NOCASE,
	caps TEXT COLLATE NOCASE
);
	]], {
		BanRanges=[[
INSERT INTO Bans0(
	id,
	startaddr,
	endaddr,
	expires,
	revoked,
	bantime,
	playername,
	comment,
	server,
	bannedby,
	caps
) SELECT
	id,
	startaddr,
	endaddr,
	expires,
	0,
	bantime,
	name,
	comment,
	NULL,
	bannedby,
	REPLACE('badcap:'||flags, ' ', ' badcap:')
FROM BanRanges;

INSERT INTO Bans0(
	id,
	startaddr,
	endaddr,
	expires,
	revoked,
	bantime,
	playername,
	comment,
	server,
	bannedby,
	caps
) SELECT
	id,
	startaddr,
	endaddr,
	expires,
	1,
	bantime,
	name,
	comment,
	NULL,
	bannedby,
	REPLACE('badcap:'||flags, ' ', ' badcap:')
FROM ArchivedBans;

UPDATE sqlite_sequence SET seq = (SELECT seq FROM sqlite_sequence where name = 'BanRanges') WHERE name = 'Bans0';
DELETE FROM sqlite_sequence WHERE name = 'BanRanges';
DROP TABLE BanRanges;
DROP TABLE ArchivedBans;
		]]
	});

	data_ver = ldb.prepare_1ret(db, "data_ver", "PRAGMA data_version;");

	sel = ldb.prepare_xret(db, "sel", "SELECT id, caps, expires FROM "..tbl.." WHERE ? BETWEEN startaddr AND endaddr AND expires > unixepoch() AND revoked = 0;");

	ban = ldb.prepare_0ret(db, "ban", "INSERT INTO "..tbl.."(id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps) VALUES(NULL, ?, ?, unixepoch() + ?, 0, unixepoch(), ?, ?, ?, ?, ?);");
	revoke = ldb.prepare_0ret(db, "revoke", "UPDATE "..tbl.." SET revoked = 1 WHERE id = ?;");
	unrevoke = ldb.prepare_0ret(db, "unrevoke", "UPDATE "..tbl.." SET revoked = 0 WHERE id = ?;");

	queryaddr = ldb.prepare_xret(db, "queryaddr", "SELECT id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps FROM "..tbl.." WHERE ? BETWEEN startaddr AND endaddr OR ? BETWEEN startaddr AND endaddr OR startaddr BETWEEN ? and ? OR endaddr BETWEEN ? and ?;");
	queryname = ldb.prepare_xret(db, "queryname", "SELECT id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps FROM "..tbl.." WHERE playername = ?;");
	querycomment = ldb.prepare_xret(db, "queryname", "SELECT id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps FROM "..tbl.." WHERE comment = ?;");

	for i in piditer(PID_BROADCAST) do
		check_bans(i);
	end
end

function mod.on_unload()
	-- TODO: *don't* remove caps on unload?
	for i in piditer(PID_BROADCAST) do
		local oldcaps = {};

		for id, tbl in pairs(bans[i]) do
			for cap in pairs(tbl.caps) do
				oldcaps[cap] = true;
			end
		end

		sync_bans_with_caps(i, oldcaps, {});
	end

	ldb.close(db);
end

local lastdataver = nil;
local lasttime = nil;
function mod.after.tick()
	-- PRAGMA data_version changes whenever another process modifies the database
	local ver = data_ver()[1];
	if (ver ~= lastdataver) then
		for i in piditer(PID_BROADCAST) do
			check_bans(i);
		end

		lastdataver = ver;
	end

	local now = os.time();
	if (now ~= lasttime) then
		tick_bans(now);

		lasttime = now;
	end
end

function mod.on_any_connect(pid)
	attemptingconnect = true;
	check_bans(pid);
	attemptingconnect = false;

	if (has_cap(pid, "badcap:ban")) then
		-- TODO: pretty-print the ipaddr
		log("%s:%u (#%u) attempted to connect but is banned", get_ipaddr(pid), get_udp_port(pid), pid);
		disconnect_now(pid, 1);
		return;
	end

	mod.next.on_any_connect(pid);
end

function bans_ban_player(pid, banpid, duration, comment, caps)
	local addr = get_ipaddr(banpid);
	local banner;

	if (auth_users and auth_users[pid]) then
		banner = "@"..auth_users[pid];
	else
		banner = get_name(pid);
	end

	ban(addr, addr, duration, get_name(banpid), comment, bans_server_name, banner, caps);
	local id = db.con:last_insert_rowid();

	-- We iterate over everyone instead of just the banpid because
	-- the banpid's ipaddr may have multiple connections at once.
	for i in piditer(PID_BROADCAST) do
		check_bans(i);
	end

	-- Hopefully this plays well with self-bans. . .
	l10n_send_chat(pid, ban_id_msg, {id=id});
end

function bans_ban_addr(pid, starta, enda, playername, duration, comment, caps)
	local banner;

	if (auth_users and auth_users[pid]) then
		banner = "@"..auth_users[pid];
	else
		banner = get_name(pid);
	end

	ban(starta, enda, duration, playername, comment, bans_server_name, banner, caps);
	local id = db.con:last_insert_rowid();

	-- We iterate over everyone instead of just the banpid because
	-- the banpid's ipaddr may have multiple connections at once.
	for i in piditer(PID_BROADCAST) do
		check_bans(i);
	end

	-- Hopefully this plays well with self-bans. . .
	l10n_send_chat(pid, ban_id_msg, {id=id});
end

local cmd = {name="ban", caps="bans", fakepid=true, usage="player duration comment...", desc="Ban a naughty player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 3);

	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = table.concat(argv, " ", 3);

	bans_ban_player(pid, banpid, duration, comment, bans_default_caps);
end
register_command(cmd, mod);

local cmd = {name="bcban", caps="bcban", fakepid=true, usage="player duration comment badcaps...", desc="Ban a naughty player with custom badcaps."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 4);

	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = argv[3];
	local badcaps = "badcap:"..string.gsub(table.concat(argv, " ", 4), " +", " badcap:");

	bans_ban_player(pid, banpid, duration, comment, badcaps);
end
register_command(cmd, mod);

local cmd = {name="capban", caps="capban", fakepid=true, usage="player duration comment caps...", desc="Ban a naughty (nice?) player with custom caps."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 4);

	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = argv[3];
	local caps = table.concat(argv, " ", 4);

	bans_ban_player(pid, banpid, duration, comment, caps);
end
register_command(cmd, mod);

-- TODO: allow passing player name to it (instead of hardcoded nil)?
local cmd = {name="banip", caps="bans", fakepid=true, usage="range duration comment...", desc="Ban a naughty IPv4 address or CIDR range."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 3);

	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = table.concat(argv, " ", 3);

	bans_ban_addr(pid, starta, enda, nil, duration, comment, bans_default_caps);
end
register_command(cmd, mod);

local cmd = {name="bcbanip", caps="bcban", fakepid=true, usage="range duration comment badcaps...", desc="Ban a naughty IPv4 address or CIDR range with custom badcaps."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 4);

	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = argv[3];
	local badcaps = "badcap:"..string.gsub(table.concat(argv, " ", 4), " +", " badcap:");

	bans_ban_addr(pid, starta, enda, nil, duration, comment, badcaps);
end
register_command(cmd, mod);

local cmd = {name="capbanip", caps="capban", fakepid=true, usage="range duration comment caps...", desc="Ban a naughty (nice?) IPv4 address or CIDR range with custom caps."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 4);

	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local duration = get_arg_time("duration", pid, cmd, argv[2]);
	local comment = argv[3];
	local caps = table.concat(argv, " ", 4);

	bans_ban_addr(pid, starta, enda, nil, duration, comment, caps);
end
register_command(cmd, mod);

local cmd = {name="unban", caps="bans", fakepid=true, usage="id", desc="Revoke a given ban ID."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local id = get_arg_num_range("id", pid, cmd, argv[1], 0, math.huge);

	if (revoke(id) ~= 1) then
		l10n_send_chat(pid, ban_id_invalid_msg);
		return;
	end

	for i in piditer(PID_BROADCAST) do
		check_bans(i);
	end
end
register_command(cmd, mod);

local cmd = {name="reban", caps="bans", fakepid=true, usage="id", desc="Unrevoke a given ban ID."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local id = get_arg_num_range("id", pid, cmd, argv[1], 0, math.huge);

	if (unrevoke(id) ~= 1) then
		l10n_send_chat(pid, ban_id_invalid_msg);
		return;
	end

	for i in piditer(PID_BROADCAST) do
		check_bans(i);
	end
end
register_command(cmd, mod);

-- TODO: print addr range?
local function print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps)
	l10n_send_chat(
		pid,
		revoked ~= 0 and query_msg_revoked or (
			now >= expires and query_msg_expired or query_msg
		),
		{
			id=id,
			name=playername or "",
			date=os.date("!%Y-%m-%dT%H:%M:%SZ", bantime),
			banner=bannedby,
			comment=comment,
			caps=caps
		}
	);
end

local cmd = {name="querybanaddr", caps="bans", fakepid=true, usage="range", desc="Query all unexpired, unrevoked bans matching a given IPv4 address or CIDR range."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in queryaddr(starta, enda, starta, enda, starta, enda) do
		if (revoked == 0 and now < expires) then
			print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
		end
	end
end
register_command(cmd, mod);

local cmd = {name="querybanname", caps="bans", fakepid=true, usage="name", desc="Query all unexpired, unrevoked bans matching a given player name (case-insensitive)."};
function cmd.func(pid, argv, msg)
	cmd_assert(pid, cmd, #argv == 1);
	local name = argv[1];
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in queryname(name) do
		if (revoked == 0 and now < expires) then
			print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
		end
	end
end
register_command(cmd, mod);

local cmd = {name="querybancomment", caps="bans", fakepid=true, usage="comment", desc="Query all unexpired, unrevoked bans matching a given comment (case-insensitive)."};
function cmd.func(pid, argv, msg)
	cmd_assert(pid, cmd, #argv == 1);
	local comment = argv[1];
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in querycomment(comment) do
		if (revoked == 0 and now < expires) then
			print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
		end
	end
end
register_command(cmd, mod);

local cmd = {name="queryallbanaddr", caps="bans", fakepid=true, usage="range", desc="Query all bans matching a given IPv4 address or CIDR range."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in queryaddr(starta, enda, starta, enda, starta, enda) do
		print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
	end
end
register_command(cmd, mod);

local cmd = {name="queryallbanname", caps="bans", fakepid=true, usage="name", desc="Query all bans matching a given player name (case-insensitive)."};
function cmd.func(pid, argv, msg)
	cmd_assert(pid, cmd, #argv == 1);
	local name = argv[1];
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in queryname(name) do
		print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
	end
end
register_command(cmd, mod);

local cmd = {name="queryallbancomment", caps="bans", fakepid=true, usage="comment", desc="Query all bans matching a given comment (case-insensitive)."};
function cmd.func(pid, argv, msg)
	cmd_assert(pid, cmd, #argv == 1);
	local comment = argv[1];
	local now = os.time();

	for id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps in querycomment(comment) do
		print_query(pid, now, id, startaddr, endaddr, expires, revoked, bantime, playername, comment, server, bannedby, caps);
	end
end
register_command(cmd, mod);

return mod;
