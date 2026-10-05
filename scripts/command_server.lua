-- command_server.lua -- Extract the server's internet-facing aos:// address
local mod = init_mod();
local bit = require("bit");
local sock = nil;

getcfg("server_aos_address", nil);
getcfg("server_get_ip", false);
getcfg("server_get_ip_domain", "ipv4.icanhazip.com");

local unknown_msg = {
	en="Unknown address"
};

local function on_connect()
	return 1;
end

local function null()
end

local function on_line(sock, cid, line)
	line = string.gsub(line, "\r$", "", 1);

	-- Hopefully the IP getty service doesn't try to
	-- return something silly like 314.2000.86.0 :p
	local a, b, c, d = string.match(line, "^(%d+)%.(%d+)%.(%d+)%.(%d+)$");
	if (a ~= nil) then
		server_aos_address = string.format(
			"aos://%u:%u",
			bit.bor(bit.lshift(d, 24), bit.lshift(c, 16), bit.lshift(b, 8), a),
			ENET_PORT
		);

		log("command_server: got IPv4: %s", line);
		sock_close(sock);
		sock = nil;
	end
end

function mod.on_load()
	if (server_aos_address ~= nil) then
		return;
	end

	if (ENET_ADDRESS ~= 0) then
		-- If explicitly bound, just return that, even if it's not
		-- technically guaranteed "internet-facing". It's probably
		-- what's wanted, anyway.

		-- TODO: endian-swap ENET_ADDRESS?
		server_aos_address = string.format("aos://%u:%u", ENET_ADDRESS, ENET_PORT);
		return;
	end

	-- If bound to INADDR_ANY, the server's address has to be fetched
	-- from some external service, since the kernel won't tell us and
	-- we might be behind NAT anyway.
	if (server_get_ip) then
		sock = sock_new_tcp_client(
			server_get_ip_domain,
			80,
			{on_line=on_line, on_connect=on_connect, on_disconnect=null}
		);

		sock_send_broadcast(
			sock,
			"GET / HTTP/1.0\r\n"..
			"Host: "..server_get_ip_domain.."\r\n"..
			"\r\n"
		);
	end
end

function mod.on_unload()
	if (sock) then
		sock_close(sock);
	end
end

local cmd = {name="server", fakepid=true, desc="Print the server's address."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	if (server_aos_address) then
		server_msg(pid, server_aos_address);
	else
		l10n_send_chat(pid, unknown_msg);
	end
end
register_command(cmd, mod);

return mod;
