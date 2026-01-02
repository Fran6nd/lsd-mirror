-- auth.lua -- Leak your passwords to the world with /login
local mod = {};
local sql = require "lsqlite3";
local sodium = require "luasodium";
local totp = require "lib_totp";
local db;
local stmt = {};

getcfg("auth_groups", {
	guard = {
		"cmd:advance",
		"some_limited_ban_cap"
	},
	mod = {
		"guard",
		"jp",
		"noclip"
	},
	admin = {
		"all"
	},
	-- The nerd group is the most powerful group here
	nerd = {
		"exec",
		"modutils"
	}
});

-- Key is a pid
-- user is a string with the username
-- groups is a table with groups/caps as keys; values are all set to true
-- if groups.all is set, all capabilities are granted
local user = {};
local groups = {};
-- Computed from groups
local caps = {};

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
	db, code, msg = sql.open("rw/auth.db");

	if (db == nil) then
		error("sql.open: " .. msg);
	end

	db:busy_timeout(100);

	-- group determines what capabilities the user has, and so does caps
	-- TODO: use base32 for totp?
	-- TODO: do i need id or WITHOUT ROWID?
	db:exec[[
		PRAGMA journal_mode = WAL;
		PRAGMA temp_store = memory;
		CREATE TABLE IF NOT EXISTS Users(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE COLLATE NOCASE, groups TEXT COLLATE NOCASE, caps TEXT COLLATE NOCASE, password TEXT, totp BLOB, totptype TEXT, totpinterval INTEGER);
	]];

	-- TODO: instead of this VALUES array, use a map?
	-- TODO: merge groups and caps?
	createstmt("useradd",   "INSERT INTO Users VALUES(NULL, ?, ?, ?, ?, ?, ?, ?);");
	---- selects name for case-sensitivity whatever? why would the name not be lowercase, actually?
	--createstmt("id",        "SELECT name, groups, caps FROM Users WHERE name = ?;");
	createstmt("getpasswd", "SELECT name, groups, caps, password, totp, totptype, totpinterval FROM Users WHERE name = ?;");
	createstmt("setpasswd", "UPDATE USERS SET password = ? WHERE name = ?;");
	createstmt("settotp", "UPDATE USERS SET totp = ?, totptype = ?, totpinterval = ? WHERE name = ?;");
end

function mod.on_unload()
	user = {};
	groups = {};

	for _,y in pairs(stmt) do
		y:finalize();
	end

	if (db ~= nil) then
		db:close();
	end
end

function has_cap(pid, cap)
	return groups[pid] ~= nil and (groups[pid].all or groups[pid][cap]);
end

function grant_cap(pid, cap)
	if (groups[pid] == nil) then
		groups[pid] = {};
	end

	groups[pid][cap] = true;
end

function drop_cap(pid, cap)
	groups[pid][cap] = nil;
end

local function get_cmd_canonical_name(cmd)
	if (type(cmd.name) == "table") then
		return cmd.name[1];
	end

	return cmd.name;
end

-- Abusing register to interface cleanly with commands.lua
function mod.try_run_command(cmd, pid, argv, msg)
	if (cmd.caps ~= nil) then
		if (groups[pid] == nil) then
			send_chat(pid, "You need to login to run that command.", 2, 0);
			return;
		end
		if (not has_cap(pid, cmd.caps) and not has_cap(pid, "cmd:"..get_cmd_canonical_name(cmd))) then
			send_chat(pid, "You need the "..cmd.caps.." capability to run that command.", 2, 0)
			return;
		end
	end

	next_call("try_run_command", mod.try_run_command)(cmd, pid, argv, msg);
end

function mod.can_see_command(pid, cmd)
	if (not next_call("can_see_command", mod.can_see_command)(pid, cmd)) then
		return false;
	end

	return cmd.caps == nil or has_cap(pid, cmd.caps) or has_cap(pid, "cmd:"..get_cmd_canonical_name(cmd));
end

-- TODO: should i rewrite half of this stuff in C and use sodium's secure memory functions?
-- #key should be 32
local function hmac_sha256(key, msg)
	return sodium.crypto_auth_hmacsha256(msg, key);
end

local function hmac_sha512(key, msg)
	return sodium.crypto_auth_hmacsha512(msg, key);
end

local hmac = {};
hmac["hmac-sha1"] = totp.hmac_sha1;
hmac["hmac-sha256"] = hmac_sha256;
hmac["hmac-sha512"] = hmac_sha512;

local cmd = {name="register", usage="name password", desc="Create an account."};
function cmd.func(pid, argv)
	if (#argv ~= 2) then
		-- TODO: auto-generate usage?
		send_usage(pid, cmd);
		return;
	end

	local name = argv[1];
	local pwd = argv[2];

	-- TODO: do i need to handle sodium errors?
	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	-- TODO: pass NULL to this stuff?
	stmt.useradd:bind_values(name, "", "", sodium.crypto_pwhash_str(pwd, sodium.crypto_pwhash_OPSLIMIT_INTERACTIVE, sodium.crypto_pwhash_MEMLIMIT_INTERACTIVE), "", "", 0);
	if (stmt.useradd:step() ~= sql.DONE) then
		stmt.useradd:reset();
		send_chat(pid, "Can't register; are you sure that name isn't taken?", 2, 0);
		return;
	end
	stmt.useradd:reset();
end
register_command(cmd);

-- TODO: -> totp_gen?
local cmd = {name="set_totp", caps="login", desc="Configure TOTP for your account."};
function cmd.func(pid, argv)
	if (#argv ~= 0) then
		send_usage(pid, cmd);
		return;
	end

	-- TODO: tie into logins

	-- TODO: mush it all into one string?
	--local totp_key = sodium.randombytes_buf(32);
	local totp_key = sodium.crypto_auth_hmacsha256_keygen();
	local totp_type = "hmac-sha1";
	local totp_interval = 30;

	stmt.settotp:reset();
	stmt.settotp:bind_values(totp_key, totp_type, totp_interval, user[pid]);
	code = stmt.settotp:step();
	if (code ~= sql.DONE) then
		error("settotp:step: "..code);
	end

	local encodedkey = totp.base32enc(totp_key);

	send_chat(pid, string.upper(totp_type)..", "..totp_interval.." s, key: "..encodedkey, 2, 0)
	-- TODO: verify that the user actually did something with the key
	-- TODO: password reset resets TOTP too?
	send_chat(pid, totp.gen_code(totp_key, os.time()/totp_interval, hmac[totp_type]), 2, 0);
	send_chat(pid, totp.gen_code(totp_key, os.time()/totp_interval+1, hmac[totp_type]), 2, 0);
	send_chat(pid, totp.gen_code(totp_key, os.time()/totp_interval+2, hmac[totp_type]), 2, 0);
end
register_command(cmd);

local cmd = {name="login", desc="Login to an account."};
function cmd.func(pid, argv)
	if (#argv ~= 2 and #argv ~= 3) then
		-- TODO: totp
		send_usage(pid, cmd);
		return;
	end

	local name = argv[1];
	local pwd = argv[2];
	local otp = argv[3];
	local vals;

	-- TODO: do i need to handle sodium errors?
	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	stmt.getpasswd:reset();
	stmt.getpasswd:bind_values(name);
	local code = stmt.getpasswd:step();
	if (code ~= sql.DONE and code ~= sql.ROW) then
		error("getpasswd:step: "..code);
	end
	if (code == sql.DONE) then
		send_chat(pid, "User not found.", 2, 0);
		return;
	end
	vals = stmt.getpasswd:get_values();
	-- TODO: should i reset after?

	if (not sodium.crypto_pwhash_str_verify(vals[4], pwd)) then
		send_chat(pid, "NO", 2, 0);
		return;
	end

	if (vals[5] ~= "") then
		local totp_key = vals[5];
		local totp_type = vals[6];
		local totp_interval = vals[7];
		local ctr = os.time()/totp_interval;

		for i=-1,0,1 do
			if (tonumber(otp) == totp.gen_code(totp_key, ctr+i, hmac[totp_type])) then
				goto okay;
			end
		end

		send_chat(pid, "NO", 2, 0);
		return;
	elseif (otp ~= nil) then
		send_chat(pid, "NO", 2, 0);
		return;
	end

	::okay::
	send_chat(pid, "OK", 2, 0);

	user[pid] = vals[1];
	-- TODO: caps-groups merge
	groups[pid] = {login=true};
	-- TODO: update commands gmatch to %S+, or update this one instead
	for x in string.gmatch(vals[2] .. " " .. vals[3], "%S+") do
		groups[pid][x] = true;
	end
end
register_command(cmd);

local cmd = {name="logout", caps="login", desc="Log out of your account."};
function cmd.func(pid)
	user[pid] = nil;
	groups[pid] = nil;
end
register_command(cmd);

local cmd = {name="id", caps="login", desc="Print your account name and groups."};
function cmd.func(pid, argv)
	local strgroups = "";
	local delim = "";
	for x,_ in pairs(groups[pid]) do
		strgroups = strgroups .. delim .. x;
		delim = ", ";
	end
	send_chat(pid, string.format("uid=%s groups=%s", user[pid], strgroups), 2, 0);
end
register_command(cmd);

local cmd = {name="totp", caps="test"};
function cmd.func(pid, argv)
	send_chat(pid, totp.gen_code("Hello!\xde\xad\xbe\xef", os.time()/30, totp.hmac_sha1), 2, 0);
end
register_command(cmd);

local cmd = {name="totp-512", caps="test"};
function cmd.func(pid, argv)
	send_chat(pid, totp.gen_code("\x94\xed\x83\x2c\xed\x3a\xd3\xd1\xf0\x51\xd8\x1e\x19\x27\xb4\xae\x43\xba\x74\x3a\xb3\x01\xb4\x90\x52\xa0\x3a\xc4\x7a\x0d\x3a\x4a", os.time()/30, hmac_sha512), 2, 0);
end
register_command(cmd);

return mod;
