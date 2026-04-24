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

local function on_disconnect(sock, pid)
	if (sock.cons[pid].wshdr) then
		log("wscon: tcp connection #%u closed", pid);
	else
		log("wscon: disconnected: %s (#%u)", get_name(pid), pid);
	end
	free_fakepid(pid);
end

local function after_connect(sock, pid)
	-- TODO: log ipaddr (v4/v6)
	log("wscon: got tcp connection (#%u)", pid);
end

local function on_ws_recv(sock, pid)
	local line = sock.cons[pid].wsbuf:get();
	if (#line ~= 0) then
		log("wscon: %s (#%u): /%s", get_name(pid), pid, line);
		websock_send_con(sock.cons[pid], "> "..line.."\n");
		handle_command(pid, line, true);
	end
end

local function on_ws_connect(sock, pid)
	log("wscon: connected: %s (#%u)", get_name(pid), pid);
	on_fakepid_connect(pid);
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
<label for=stdin>Enter a command:</label>
<input autofocus placeholder="cmds 0" id=stdin>
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

var hist = [""];
var histidx = 0;

function output(str) {
	stderr.value += str;
	stderr.scrollTop = stderr.scrollHeight;
}

function open(event) {output("Connected\n");}
function close(event) {output("Disconnected: " + event.code + " (" + (event.wasClean ? "clean" : "unclean") + ")\n");}
function error(event) {output("Error\n");}
function message(event) {output(event.data);}

function submit(event) {
	event.preventDefault();
	if (stdin.value != "")
		sock.send(stdin.value);

	hist[histidx] = stdin.value;
	if (hist[hist.length-1] != "")
		hist.push("");

	histidx = hist.length-1;

	stdin.value = "";
}

function keydown(event) {
	switch (event.key) {
	case "ArrowUp":
		hist[histidx] = stdin.value;
		if (--histidx < 0)
			histidx = 0;
		break;
	case "ArrowDown":
		hist[histidx] = stdin.value;
		if (++histidx >= hist.length)
			histidx = hist.length-1;
		break;
	default:
		return;
	}

	stdin.value = hist[histidx];
	event.preventDefault();
}

sock.addEventListener("close", close, {passive: true});
sock.addEventListener("error", error, {passive: true});
sock.addEventListener("message", message, {passive: true});
sock.addEventListener("open", open, {passive: true});

input.addEventListener("submit", submit);
input.addEventListener("keydown", keydown);
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
	resize: none;
	flex-grow: 1;
	word-break: break-all;
}

#input {
	display: grid;
	border-top: none !important;
}

#stdin {
	border: none;
	/* #stdin ignores :root's font-family */
	font-family: monospace;
}

#stderr, #input {
	border: 2px solid #057;
}

#stdin, #stderr {
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
	sock = sock_new_tcp(websock_console_addr, websock_console_port);
	sock.on_recv = websock_recv;
	sock.on_connect = on_connect;
	sock.on_disconnect = on_disconnect;
	sock.after_connect = after_connect;
	sock.on_ws_recv = on_ws_recv;
	sock.on_ws_connect = on_ws_connect;
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
-- TODO: if something decides to piditer over PID_BROADCAST before this, what happens?
function mod.send_chat(pid, msg, type, from)
	-- TODO: one conpid per connection? OPTIONAL?
	if (sock.cons[pid]) then
		websock_send_con(sock.cons[pid], msg.."\n");
		return;
	end

	-- TODO: add player ID, probably don't show if has log cap, show connect/disconnect, maybe spectator team chat
	if (pid == PID_BROADCAST) then
		for pid, con in pairs(sock.cons) do
			if (from < MAX_PLAYERS and type ~= 2) then
				websock_send_con(con, string.format("%s: %s\n", get_name(from), msg));
			else
				websock_send_con(con, msg.."\n");
			end
		end
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
