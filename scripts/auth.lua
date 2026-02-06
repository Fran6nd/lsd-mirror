-- auth.lua -- Leak your passwords to the world with /login
require "lib_l10n";
local mod = {after={}};
local sql = require "lsqlite3";
local sodium = require "luasodium";
local totp = require "lib_totp";
local db;
local stmt = {};

local granted = {};
auth_users = {};

getcfg("auth_db", "rw/auth.db");

-- TODO: need to associate account name with fakepid's

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
	db, code, msg = sql.open(auth_db);

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
		CREATE TABLE IF NOT EXISTS Users(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE COLLATE NOCASE, caps TEXT COLLATE NOCASE, password TEXT, totp BLOB, totptype TEXT, totpinterval INTEGER);
	]];

	-- TODO: instead of this VALUES array, use a map?
	createstmt("useradd",   "INSERT INTO Users VALUES(NULL, ?, ?, ?, ?, ?, ?);");
	---- selects name for case-sensitivity whatever? why would the name not be lowercase, actually?
	--createstmt("id",        "SELECT name, groups, caps FROM Users WHERE name = ?;");
	createstmt("getpasswd", "SELECT name, caps, password, totp, totptype, totpinterval FROM Users WHERE name = ?;");
	createstmt("setpasswd", "UPDATE USERS SET password = ? WHERE name = ?;");
	createstmt("settotp", "UPDATE USERS SET totp = ?, totptype = ?, totpinterval = ? WHERE name = ?;");
	createstmt("setcaps", "UPDATE USERS SET caps = ? WHERE name = ?;");

	granted = {};
	auth_users = {};
end


local function drop_granted(pid)
	if (granted[pid]) then
		for _,y in ipairs(granted[pid]) do
			drop_cap(pid, y);
		end
	end
end

function mod.after.on_disconnect(pid)
	drop_granted(pid);
	auth_users[pid] = nil;
	granted[pid] = nil;
end

function mod.on_unload()
	for _,y in pairs(stmt) do
		y:finalize();
	end

	if (db ~= nil) then
		db:close();
	end

	for i in piditer(PID_BROADCAST) do
		drop_granted(i);
	end
end

local login_first_msg = {
	en="You need to login to run that command."
};

function mod.try_run_command(cmd, pid, argv, msg)
	if (cmd.caps ~= nil and not has_cap(pid, "login") and not has_cap(pid, cmd.caps)) then
		l10n_send_chat(pid, login_first_msg);
		return;
	end
	next_call("try_run_command", mod.try_run_command)(cmd, pid, argv, msg);
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

local password_too_long_msg = {
	en="Don't make the password that long or you might not be able to change it! Try <= %(max) chars."
};

-- TODO: ensure that name taken is really the reason register doesn't work
local name_taken_msg = {
	en="Can't register; are you sure that name isn't taken?"
};

-- TODO: make register a default cap?
local cmd = {name="register", caps="register", fakepid=true, usage="name password", desc="Create an account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2);

	local name = argv[1];
	local pwd = argv[2];

	-- OpenSpades chat length limit is 255 (maybe +|- 1 depending on how you count) UTF-8 bytes
	if (#pwd > 128) then
		l10n_send_chat(pid, password_too_long_msg, {max=128});
		return;
	end

	-- TODO: do i need to handle sodium errors?
	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	-- TODO: pass NULL to this stuff?
	stmt.useradd:bind_values(name, "", sodium.crypto_pwhash_str(pwd, sodium.crypto_pwhash_OPSLIMIT_INTERACTIVE, sodium.crypto_pwhash_MEMLIMIT_INTERACTIVE), "", "", 0);
	if (stmt.useradd:step() ~= sql.DONE) then
		stmt.useradd:reset();
		l10n_send_chat(pid, name_taken_msg);
		return;
	end
	stmt.useradd:reset();
end
register_command(cmd);

-- TODO: -> totp_gen?
local cmd = {name="settotp", caps="login", fakepid=true, desc="Configure TOTP for your account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

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

-- TODO: use
local function exec_stmt(name, ...)
	stmt[name]:reset();
	stmt[name]:bind_values(...);
	code = stmt[name]:step();
	if (code ~= sql.DONE) then
		error(name..":step: "..code);
	end
end

local cmd = {name="authcaps", caps="authcaps", fakepid=true, usage="user caps...", desc="Set an account's capabilities."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 1);

	-- TODO: update caps of connected player with user[player] == arg.user
	-- TODO: pretty errors for nonexistent user
	exec_stmt("setcaps", table.concat(argv, " ", 2), argv[1]);
end
register_command(cmd);

local cmd = {name="chpasswd", caps="login", fakepid=true, desc="Change your account's password."};
function cmd.func(pid, argv)
	-- TODO
end
register_command(cmd);

-- TODO: should it just pretend it's a bad password? only if registration is locked though
local no_user_msg = {
	en="User not found."
};

local bad_login_msg = {
	en="Bad password or TOTP."
};

local cmd = {name="login", fakepid=true, usage="name password [otp]", desc="Login to an account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2 or #argv == 3);

	local name = argv[1];
	local pwd = argv[2];
	local otp = argv[3];
	local vals;

	-- TODO: do i need to handle sodium errors?
	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	-- TODO: add cooldown to /login for the same reason and to prevent brute force
	stmt.getpasswd:reset();
	stmt.getpasswd:bind_values(name);
	local code = stmt.getpasswd:step();
	if (code ~= sql.DONE and code ~= sql.ROW) then
		error("getpasswd:step: "..code);
	end
	if (code == sql.DONE) then
		l10n_send_chat(pid, no_user_msg);
		return;
	end
	vals = stmt.getpasswd:get_values();
	-- TODO: should i reset after?

	if (not sodium.crypto_pwhash_str_verify(vals[3], pwd)) then
		l10n_send_chat(pid, bad_login_msg);
		return;
	end

	if (vals[4] ~= "") then
		local totp_key = vals[4];
		local totp_type = vals[5];
		local totp_interval = vals[6];
		local ctr = os.time()/totp_interval;

		for i=-1,0,1 do
			if (tonumber(otp) == totp.gen_code(totp_key, ctr+i, hmac[totp_type])) then
				goto okay;
			end
		end

		l10n_send_chat(pid, bad_login_msg);
		return;
	elseif (otp ~= nil) then
		l10n_send_chat(pid, bad_login_msg);
		return;
	end

	::okay::
	-- TODO: send id?
	send_chat(pid, "OK", 2, 0);

	-- TODO: remove caps from table
	auth_users[pid] = vals[1];
	drop_granted(pid);
	granted[pid] = {};
	table.insert(granted[pid], "login");
	grant_cap(pid, "login");
	for x in string.gmatch(vals[2], "%S+") do
		table.insert(granted[pid], x);
		grant_cap(pid, x);
	end
end
register_command(cmd);

local cmd = {name="logout", caps="login", fakepid=true, desc="Log out of your account."};
function cmd.func(pid)
	drop_granted(pid);
	auth_users[pid] = nil;
	granted[pid] = nil;
end
register_command(cmd);

-- TODO: move user to caps so we don't clear everything on unload?
local cmd = {name="id", caps="login", fakepid=true, desc="Print your account name and groups."};
function cmd.func(pid, argv)
	send_chat(pid, string.format("uid=%s groups=[%s] computed=[%s]", auth_users[pid], get_cap_groups(pid), get_caps(pid)), 2, 0);
end
register_command(cmd);

return mod;
