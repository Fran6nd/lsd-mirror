-- auth.lua -- Leak your passwords to the world with /login
local mod = {};
local sql = require "lsqlite3";
local sodium = require "luasodium";
local totp = require "lib_totp";

-- TODO: should i rewrite half of this stuff in C and use sodium's secure memory functions?
-- #key should be 32
local function hmac_sha512(key, msg)
	return sodium.crypto_auth_hmacsha512(msg, key);
end

local cmd = {name="totp"};
function cmd.func(pid, argv)
	send_chat(pid, totp.gen_code("Hello!\xde\xad\xbe\xef", os.time()/30, totp.hmac_sha1), 2, 0);
end
register_command(cmd);

local cmd = {name="totp-512"};
function cmd.func(pid, argv)
	send_chat(pid, totp.gen_code("\x94\xed\x83\x2c\xed\x3a\xd3\xd1\xf0\x51\xd8\x1e\x19\x27\xb4\xae\x43\xba\x74\x3a\xb3\x01\xb4\x90\x52\xa0\x3a\xc4\x7a\x0d\x3a\x4a", os.time()/30, hmac_sha512), 2, 0);
end
register_command(cmd);

return mod;
