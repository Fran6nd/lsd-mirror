-- if_websocket.lua -- Websocket interface that may or may not blow a hole in your wall
local mod = init_mod();
local sock;

getcfg("if_websocket_addr", nil);
getcfg("if_websocket_port", ENET_PORT);

local tcp_con_log_msg = {
	en="ifwsock: got tcp connection %(addr) (#%(pid))"
};

local tcp_con_closed_log_msg = {
	en="ifwsock: tcp connection %(addr) (#%(pid)) closed"
};

local con_log_msg = {
	en="ifwsock: connected: (#%(pid))"
};

local disconnected_log_msg = {
	en="ifwsock: disconnected: %(addr) (#%(pid))"
};

local function xon_connect(sock, con)
	-- TODO: only assign pid on *websocket* connect, so random scraperbots don't
	-- waste a whole pid (and so useful html can be displayed to non-websocketeers)
	local pid = assign_new_pid();
	websock_init_con(con);
	return pid;
end

local function xon_disconnect(sock, pid)
	if (sock.cons[pid].wshdr) then
		l10n_log(tcp_con_closed_log_msg, {addr=sock_tcp_getname(sock.cons[pid]), pid=pid});
	else
		l10n_log(disconnected_log_msg, {addr=sock_tcp_getname(sock.cons[pid]), pid=pid});
	end
	on_disconnect(pid);
end

local function xafter_connect(sock, pid)
	l10n_log(tcp_con_log_msg, {addr=sock_tcp_getname(sock.cons[pid]), pid=pid});
	on_any_connect(pid);
end

local function on_ws_recv(sock, pid)
	local dat = sock.cons[pid].wsbuf:get();
	-- TODO: kill this construction
	if (on_any_packet(pid, dat) == 0) then
		on_sane_packet(pid, dat);
	else
		on_crap_packet(pid, dat);
	end
end

local function on_ws_connect(sock, pid)
	l10n_log(con_log_msg, {pid=pid});
end

function mod.early.send_packet(pid, data)
	for i in piditer(pid) do
		if (sock.cons[i] ~= nil) then
			websock_send_con(sock.cons[i], data, true);
		else
			mod.early.next.send_packet(i, data);
		end
	end

	return 0;
end

function mod.early.send_packet_unreliable(pid, data)
	for i in piditer(pid) do
		if (sock.cons[i] ~= nil) then
			websock_send_con(sock.cons[i], data, true);
		else
			mod.early.next.send_packet_unreliable(i, data);
		end
	end

	return 0;
end

function mod.early.disconnect(pid, reason)
	if (sock.cons[pid] ~= nil) then
		websock_disconnect(sock, pid, 4000+reason);
	else
		mod.early.next.disconnect(pid, reason);
	end
end

function mod.early.disconnect_now(pid, reason)
	if (sock.cons[pid] ~= nil) then
		websock_disconnect(sock, pid, 4000+reason);
	else
		mod.early.next.disconnect_now(pid, reason);
	end
end

function mod.early.get_ipaddr(pid)
	if (sock.cons[pid] ~= nil) then
		-- TODO: handle IPv6, probably merge this with the websock console
		local status, addr = pcall(sock_tcp_getaddr32, sock.cons[pid]);

		if (not status) then
			return 0;
		end

		return addr;
	end

	return mod.early.next.get_ipaddr(pid);
end

local WS_GOTFLAG_GET = 1
local WS_GOTFLAG_UPGRADE = 2
local WS_GOTFLAG_CONNECTION = 4
local WS_GOTFLAG_VERSION = 8

local page = [[
<!doctype html>
<html lang=en>
<title>LSd websocket interface</title>
<meta name=color-scheme content="dark light">
<style>
main{max-width: 30rem; margin: 6rem auto;}
h1{text-align: center;}
</style>
<main><article>
<h1>LSd websocket interface</h1>
<p>This page hosts a websocket interface for an LSd server.
Point a compatible client at this page to connect.
</article></main>
]];

local function ws_invalid_hdrs(sock, cid, flags)
	if (flags == WS_GOTFLAG_GET) then
		-- TODO: keep-alive?
		sock_send_con(sock.cons[cid],
			"HTTP/1.1 200 OK\r\n"..
			"Connection: close\r\n"..
			"Content-Type: text/html\r\n"..
			"Content-Length: "..#page.."\r\n\r\n"..page);
		sock.cons[cid].wsclose = true;
		rmcon(sock, cid);
	else
		websock_invalid_hdrs(sock, cid);
	end
end

function mod.on_load()
	sock = nil;
	sock = sock_new_tcp(if_websocket_addr, if_websocket_port);
	sock.on_recv = websock_recv;
	sock.on_connect = xon_connect;
	sock.on_disconnect = xon_disconnect;
	sock.after_connect = xafter_connect;
	sock.on_ws_recv = on_ws_recv;
	sock.on_ws_connect = on_ws_connect;
	sock.ws_invalid_hdrs = ws_invalid_hdrs;
end

function mod.on_unload()
	if (sock) then
		sock_close(sock);
	end
end

return mod;
