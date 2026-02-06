-- websock_console.lua -- Console that listens on a websocket
local mod = init_mod();
local sock;

getcfg("websock_console_addr", nil);
getcfg("websock_console_port", 32777);

local function on_connect(sock, con)
	local pid = new_fakepid();
	websock_init_con(con);
	return pid;
end

local function after_connect(sock, pid)
	-- TODO: log disconnect
	-- TODO: go through ws connect/disconnect callbacks, these'll log plain HTTP requests too (that's bad)
	-- TODO: do log http requests but don't give them fakepids
	log("wscon: connected: %s (#%u)", get_name(pid), pid);
	on_fakepid_connect(pid);
end

local function on_ws_recv(sock, pid)
	local line = sock.cons[pid].wsbuf:get();
	if (#line ~= 0) then
		log("wscon: %s (#%u): /%s", get_name(pid), pid, line);
		websock_send_con(sock.cons[pid], "> "..line.."\n");
		handle_command(pid, line, true);
	end
end

local WS_GOTFLAG_GET = 1
local WS_GOTFLAG_UPGRADE = 2
local WS_GOTFLAG_CONNECTION = 4
local WS_GOTFLAG_VERSION = 8

local page = [[
<!doctype html>
<html lang=en>
<title>LSd websocket console</title>
<textarea readonly id=stderr></textarea>
<form id=input>
<input id=stdin>
</form>
<script>
"use strict";

var sock;
/* TODO: file:// */
if (!(sock = (new URL(window.location.href).searchParams).get("url")) || !(sock = new WebSocket(sock)))
	sock = new WebSocket("ws"+(window.location.protocol == "https:" ? "s" : "")+"://" + window.location.host+window.location.pathname);
var stderr = document.getElementById("stderr");
var stdin = document.getElementById("stdin");
var input = document.getElementById("input");

function close(event) {
	stderr.value += "Disconnected\n";
}

function error(event) {
	stderr.value += "Error\n";
}

function message(event) {
	stderr.value += event.data;
	stderr.scrollTop = stderr.scrollHeight;
}

function open(event) {
	stderr.value += "Connected\n";
}

function submit(event) {
	event.preventDefault();
	sock.send(stdin.value);
	stdin.value = "";
}

sock.addEventListener("close", close, {passive: true});
sock.addEventListener("error", error, {passive: true});
sock.addEventListener("message", message, {passive: true});
sock.addEventListener("open", open, {passive: true});

input.addEventListener("submit", submit);
</script>
<style>
:root {
	color-scheme: dark light;
	height: 100%;
	display: flex;
	font-family: monospace;
}

body {
	max-width: 80rem;
	margin: 8rem auto;
	display: flex;
	flex-direction: column;
	width: 100%;
}

#stderr {
	border-bottom: none !important;
	resize: none;
	flex-grow: 1;
}

#stdin, #stderr {
	/* #stdin decided to ignore :root's font-family */
	font-family: monospace;
	border: 2px solid #057;
	width: 100%;
	margin: 0;
}
</style>
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
	sock = sock_new_tcp(websock_con_addr, websock_con_port);
	sock.on_recv = websock_recv;
	sock.on_connect = on_connect;
	sock.after_connect = after_connect;
	sock.on_ws_recv = on_ws_recv;
	sock.ws_invalid_hdrs = ws_invalid_hdrs;
end

function mod.on_unload()
	if (sock) then
		sock_close(sock);
	end
end

-- TODO: tee core log output
-- TODO: determine l10n language
-- TODO: l10n language set func

function mod.log(fmt, ...)
	for pid,con in pairs(sock.cons) do
		-- TODO: extend viewlog to regular players? (optionally?)
		if (has_cap(pid, "viewlog")) then
			websock_send_con(con, string.format(fmt.."\n", ...));
		end
	end
	next_call("log", mod.log)(fmt, ...);
end

-- TODO: *really* need to be able to specify early/late callchain positioning
function mod.send_chat(pid, msg, type, from)
	-- TODO: one conpid per connection? OPTIONAL?
	if (sock.cons[pid]) then
		websock_send_con(sock.cons[pid], msg.."\n");
		return;
	end
	next_call("send_chat", mod.send_chat)(pid, msg, type, from);
end

function mod.get_name(pid)
	if (sock.cons[pid]) then
		if (auth_users and auth_users[pid]) then
			return "@"..auth_users[pid];
		end

		-- TODO: should this be configurable? should i make lots of random trash configurable?
		return "@Deuce";
	end
	return next_call("get_name", mod.get_name)(pid);
end

return mod;
