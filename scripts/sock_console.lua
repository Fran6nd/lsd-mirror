-- sock_console.lua -- Run commands over a UNIX socket
local mod = init_mod();
local sock;

getcfg("sock_console", "rw/console.sock");
getcfg("sock_console_name", "@console");

function mod.on_load()
	sock = nil;
	sock = sock_new_unix(sock_console);
end

function mod.on_unload()
	if (sock) then
		sock_close(sock);
	end
end

-- TODO: tee core log output
-- TODO: determine l10n language
-- TODO: l10n language set func

function mod.late.log(fmt, ...)
	for pid,con in pairs(sock.cons) do
		sock_send_con(con, string.format(fmt.."\n", ...));
	end
	mod.late.next.log(fmt, ...);
end

function mod.late.send_chat(pid, msg, type, from)
	-- TODO: one conpid per connection? OPTIONAL?
	if (sock.cons[pid]) then
		sock_send_con(sock.cons[pid], msg.."\n");
		return;
	end
	mod.late.next.send_chat(pid, msg, type, from);
end

function mod.late.get_name(pid)
	if (sock.cons[pid]) then
		return sock_console_name;
	end
	return mod.late.next.get_name(pid);
end

return mod;
