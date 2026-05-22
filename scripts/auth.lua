-- auth.lua -- Leak your passwords to the world with /login
local mod = init_mod();
local ldb = require "lib_db";
local totp = require "lib_totp";
local sodium = require "luasodium";
local db;

if (auth_granted == nil) then
	auth_granted = pid_connected_table(function() return {} end);
end

if (auth_users == nil) then
	auth_users = pid_connected_table(nil);
end

local totp_verify = pid_connected_table(nil);

getcfg("auth_db", "rw/auth.db");
getcfg("auth_rate_passwords", true);

local login_first_msg = {
	en="You need to login to run that command."
};

local password_too_long_msg = {
	en="Don't make the password that long or you might not be able to change it! Try <= %(max) chars."
};

-- TODO: ensure that name taken is really the reason register doesn't work
local name_taken_msg = {
	en="Can't register; are you sure that name isn't taken?"
};

-- TODO: should it just pretend it's a bad password? only if registration is locked though
local no_user_msg = {
	en="User not found."
};

local bad_login_msg = {
	en="Bad password or TOTP."
};

local invalid_totp_alg_msg = {
	en="algorithm should be one of the following: SHA1, SHA256, SHA512"
};

local password_sucks_msg = {
	en="Your password is too short and hard to remember. Try using a couple of actual, *random* words instead."
};

local totpgen_totpverify_msg = {
	en="Pass your code to /totpverify to set up TOTP."
};

local totpverify_okay_msg = {
	en="Your account's TOTP configuration has been set."
};

local totpverify_bad_totp_msg = {
	en="Bad TOTP."
};

-- TODO: need to associate account name with fakepid's

local useradd, getpasswd, setpasswd, settotp, rmtotp, setcaps;
function mod.on_load()
	-- TODO: use base32 for totp?
	db = ldb.open(auth_db);

	-- 0 is the version here
	local tbl = "AuthUsers0";

	ldb.init_schema(db, [[
CREATE TABLE IF NOT EXISTS ]]..tbl..[[(
	name TEXT PRIMARY KEY COLLATE NOCASE,
	caps TEXT COLLATE NOCASE,
	password TEXT,
	totp_key BLOB,
	totp_algorithm TEXT,
	totp_period INTEGER,
	totp_digits INTEGER
) WITHOUT ROWID;
	]], {
		Users=[[
INSERT INTO AuthUsers0(
	name,
	caps,
	password,
	totp_key,
	totp_algorithm,
	totp_period,
	totp_digits
) SELECT
	name,
	caps,
	password,
	totp,
	CASE totptype
		WHEN 'hmac-sha1'   THEN 'SHA1'
		WHEN 'hmac-sha256' THEN 'SHA256'
		WHEN 'hmac-sha512' THEN 'SHA512'
	END,
	totpinterval,
	CASE WHEN totp IS NULL
		THEN NULL
		ELSE 6
	END
FROM Users;

DROP TABLE Users;
		]]
	});

	useradd   = ldb.prepare_0ret(db, "useradd",   "INSERT INTO "..tbl.." VALUES(?, '', ?, NULL, NULL, NULL, NULL);");
	getpasswd = ldb.prepare_1ret(db, "getpasswd", "SELECT name, caps, password, totp_key, totp_algorithm, totp_period, totp_digits FROM "..tbl.." WHERE name = ?;");
	setpasswd = ldb.prepare_0ret(db, "setpasswd", "UPDATE "..tbl.." SET password = ? WHERE name = ?;");
	settotp   = ldb.prepare_0ret(db, "settotp",   "UPDATE "..tbl.." SET totp_key = ?, totp_algorithm = ?, totp_period = ?, totp_digits = ? WHERE name = ?;");
	rmtotp    = ldb.prepare_0ret(db, "rmtotp",    "UPDATE "..tbl.." SET totp_key = NULL, totp_algorithm = NULL, totp_period = NULL, totp_digits = NULL WHERE name = ?;");
	setcaps   = ldb.prepare_0ret(db, "setcaps",   "UPDATE "..tbl.." SET caps = ? WHERE name = ?;");
end

function mod.on_unload()
	ldb.close(db);
end

function mod.try_run_command(cmd, pid, argv, msg)
	if (cmd.caps ~= nil and not has_cap(pid, "login") and not has_cap(pid, cmd.caps)) then
		l10n_send_chat(pid, login_first_msg);
		return;
	end
	mod.next.try_run_command(cmd, pid, argv, msg);
end

local function password_sucks(name, str)
	str = string.lower(str);
	str = string.gsub(str, "!", "");
	str = string.gsub(str, "aos", "");
	str = string.gsub(str, "password", "");
	str = string.gsub(str, "pass", "");
	str = string.gsub(str, "[aq][wz]ert?[yz]?", "");
	str = string.gsub(str, "0?1234?5?6?7?8?9?0?", "");
	str = string.gsub(str, "0?9876?5?4?3?2?1?0?", "");
	str = string.gsub(str, name, "");
	str = string.gsub(str, string.sub(name, 1, 5), "");
	return #str < 8;
end

local function hash_pass(pid, name, pwd)
	if (auth_rate_passwords and password_sucks(name, pwd)) then
		l10n_send_chat(pid, password_sucks_msg);
		cmd_exit();
	end

	-- OpenSpades chat length limit is 255 (maybe +|- 1 depending on how you count) UTF-8 bytes
	if (#pwd > 128) then
		l10n_send_chat(pid, password_too_long_msg, {max=128});
		cmd_exit();
	end

	local hash, err = sodium.crypto_pwhash_str(
                pwd,
                sodium.crypto_pwhash_OPSLIMIT_INTERACTIVE,
                sodium.crypto_pwhash_MEMLIMIT_INTERACTIVE
        );

	assert(hash ~= nil, err);
	return hash;
end

local hmac = {
	SHA1=totp.hmac_sha1,
	SHA256=sodium.crypto_auth_hmacsha256,
	SHA512=sodium.crypto_auth_hmacsha512
};

local function check_totp_okay(totp_key, totp_algorithm, totp_period, totp_digits, otp)
	local ctr = math.floor(os.time()/totp_period);

	for i=-1,0,1 do
		if (tonumber(otp) == totp.gen_code(totp_key, ctr+i, hmac[totp_algorithm], totp_digits)) then
			return true;
		end
	end

	return false;
end

local cmd = {name="register", caps="register", fakepid=true, sensitive=true, usage="name password", desc="Create an account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2);

	local name = argv[1];
	local pwd = argv[2];

	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	if (useradd(name, hash_pass(pid, name, pwd)) ~= 1) then
		l10n_send_chat(pid, name_taken_msg);
	end
end
register_command(cmd, mod);

local cmd = {name="totpgen", caps="login", fakepid=true, usage="[algorithm] [period] [digits]", desc="Setup TOTP for your account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 3);

	local totp_algorithm = argv[1] or "SHA1";
	if (hmac[totp_algorithm] == nil) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, invalid_totp_alg_msg);
		return;
	end
	local totp_period = get_arg_num_range_opt("period", pid, cmd, argv[2], 1, 300) or 30;
	local totp_digits = get_arg_num_range_opt("digits", pid, cmd, argv[3], 1, 10) or 6;
	local totp_key = sodium.crypto_auth_hmacsha256_keygen();

	local encodedkey = string.upper(totp.base32enc(totp_key));

	-- TODO: do i have to urlencode auth_users[pid]?
	server_msg(pid,
		"otpauth://totp/"..auth_users[pid]..
		"?secret="..encodedkey..
		(totp_algorithm ~= "SHA1" and "&algorithm="..totp_algorithm or "")..
		(totp_period ~= 30 and "&period="..totp_period or "")..
		(totp_digits ~= 6 and "&digits="..totp_digits or "")
	);
	l10n_send_chat(pid, totpgen_totpverify_msg);

	-- TODO: use those cap profixes as "namespaces"; /[pfx:]all/ applies to specific namespace?
	totp_verify[pid] = {totp_key, totp_algorithm, totp_period, totp_digits, auth_users[pid]};
	grant_cap(pid, "totpverify");
	auth_granted[pid].totpverify = true;
end
register_command(cmd, mod);

local cmd = {name="totprm", caps="login", fakepid=true, desc="Remove TOTP from your account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	assert(rmtotp(auth_users[pid]) == 1, "totprm: rows updated != 1");
end
register_command(cmd, mod);

local cmd = {name="totpverify", caps="totpverify", fakepid=true, sensitive=true, usage="otp", desc="Finish setting up TOTP for your account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	local otp = argv[1];

	if (not check_totp_okay(totp_verify[pid][1], totp_verify[pid][2], totp_verify[pid][3], totp_verify[pid][4], otp)) then
		l10n_send_chat(pid, totpverify_bad_totp_msg);
		return;
	end

	assert(settotp(unpack(totp_verify[pid])) == 1, "totpverify: rows updated != 1");

	drop_cap(pid, "totpverify");
	auth_granted[pid].totpverify = nil;

	l10n_send_chat(pid, totpverify_okay_msg);
end
register_command(cmd, mod);

local cmd = {name="authcaps", caps="authcaps", fakepid=true, usage="user caps...", desc="Set an account's capabilities."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 1);

	-- TODO: update caps of connected player with user[player] == arg.user
	if (setcaps(table.concat(argv, " ", 2), argv[1]) ~= 1) then
		l10n_send_chat(pid, no_user_msg);
	end
end
register_command(cmd, mod);

-- TODO: varargs?
local cmd = {name="chpasswd", caps="login", fakepid=true, sensitive=true, usage="password", desc="Change your account's password."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	assert(setpasswd(hash_pass(pid, auth_users[pid], argv[1]), auth_users[pid]) == 1, "chpasswd: rows updated != 1");
end
register_command(cmd, mod);

local function do_logout(pid)
	if (auth_granted[pid]) then
		for cap,_ in pairs(auth_granted[pid]) do
			drop_cap(pid, cap);
		end
	end

	auth_granted[pid] = {};
	auth_users[pid] = nil;
	totp_verify[pid] = nil;
end

local function do_login(pid, name, caps)
	do_logout(pid);

	auth_users[pid] = name;

	auth_granted[pid].login = true;
	grant_cap(pid, "login");

	for cap in string.gmatch(caps, "%S+") do
		if (not has_cap(pid, cap)) then
			-- Only record caps that the user doesn't already have;
			-- once the user logs out the user gets to keep the caps
			-- that user already had before logging in.
			auth_granted[pid][cap] = true;
		end
		grant_cap(pid, cap);
	end
end

local function get_id_str(pid)
	return string.format("uid=%s groups=[%s] computed=[%s]", auth_users[pid], get_cap_groups(pid), get_caps(pid));
end

local cmd = {name="login", fakepid=true, sensitive=true, usage="name password [otp]", desc="Login to an account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2 or #argv == 3);

	local name = argv[1];
	local pwd = argv[2];
	local otp = argv[3];

	-- TODO: add cooldown time to /register to prevent dos from password hashing (unless you want to spin up a new thread)
	-- TODO: add cooldown to /login for the same reason and to prevent brute force
	local vals = getpasswd(name);
	if (vals == nil) then
		l10n_send_chat(pid, no_user_msg);
		return;
	end

	if (not sodium.crypto_pwhash_str_verify(vals[3], pwd)) then
		l10n_send_chat(pid, bad_login_msg);
		return;
	end

	if (vals[4] ~= nil) then
		if (not check_totp_okay(vals[4], vals[5], vals[6], vals[7], otp)) then
			l10n_send_chat(pid, bad_login_msg);
			return;
		end
	elseif (otp ~= nil) then
		l10n_send_chat(pid, bad_login_msg);
		return;
	end

	do_login(pid, vals[1], vals[2]);
	server_msg(pid, get_id_str(pid));
end
register_command(cmd, mod);

local cmd = {name="forcelogin", caps="forcelogin", fakepid=true, usage="name", desc="Barge into an account."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	local vals = getpasswd(argv[1]);
	if (vals == nil) then
		l10n_send_chat(pid, no_user_msg);
		return;
	end

	-- TODO: send id?
	do_login(pid, vals[1], vals[2]);
	server_msg(pid, get_id_str(pid));
end
register_command(cmd, mod);

local cmd = {name="logout", caps="login", fakepid=true, desc="Log out of your account."};
function cmd.func(pid)
	do_logout(pid);
end
register_command(cmd, mod);

-- TODO: move user to caps so we don't clear everything on unload?
local cmd = {name="id", caps="login", fakepid=true, desc="Print your account name and groups."};
function cmd.func(pid, argv)
	server_msg(pid, get_id_str(pid));
end
register_command(cmd, mod);

return mod;
