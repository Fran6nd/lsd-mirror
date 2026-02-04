-- sock_console.lua -- Run commands over a UNIX socket
local mod = init_mod();
local ffi = require("ffi");
local buffer = require("string.buffer");

getcfg("sock_console", "rw/console.sock");

-- TODO: seccomp away all but accept() (except maybe for masterlist) after config loaded?
pcall(function()ffi.cdef[[
int create_unix_sock(const char *path);
int accept_unix_sock(int fd);
void close_unix_sock(int fd);
ssize_t send_unix_sock(int fd, const char *buf, size_t len);
ssize_t recv_unix_sock(int fd, char *buf, size_t len);
void *memchr(const void *s, int c, size_t n);
int unlink(const char *path);
]];end)

local un = ffi.load("./exec/libunixsock.so");
-- TODO: nuke pid with weak table
local buf;
local buflen = 1024;
local sock = -1;
local cons;

local function rmcon(pid)
	free_fakepid(pid);
	-- TODO: does closing the main socket already cover this in on_unload's case?
	un.close_unix_sock(cons[pid].fd);
	-- TODO: you can surely optimize this better
	cons[pid] = nil;
end

function mod.on_load()
	cons = {};

	buf = ffi.new("char[?]", buflen);

	-- If sock_console already exists make an attempt to remove it
	ffi.C.unlink(sock_console);
	sock = un.create_unix_sock(sock_console);
	if (sock == -1) then
		error("socket: <some error>");
	end
end

function mod.on_unload()
	for pid,con in pairs(cons) do
		rmcon(pid);
	end

	-- TODO: cleanup random buffers?
	if (sock ~= -1) then
		un.close_unix_sock(sock);
	end

	-- Hopefully this removes sock_console
	ffi.C.unlink(sock_console);
end

local function flush(con)
	local ptr, len = con.unsent:ref();
	local sentlen = un.send_unix_sock(con.fd, ptr, len);
	if (sentlen ~= -1) then
		con.unsent:skip(sentlen);
	end
end

local function send(con, text)
	con.unsent:put(text);
	flush(con);
end

-- TODO: tee core log output
-- TODO: determine l10n language
-- TODO: l10n language set func

function mod.log(fmt, ...)
	for pid,con in pairs(cons) do
		send(con, string.format(fmt.."\n", ...));
	end
	next_call("log", mod.log)(fmt, ...);
end

-- TODO: *really* need to be able to specify early/late callchain positioning
function mod.send_chat(pid, msg, type, from)
	-- TODO: one conpid per connection? OPTIONAL?
	if (cons[pid]) then
		send(cons[pid], msg.."\n");
		return;
	end
	next_call("send_chat", mod.send_chat)(pid, msg, type, from);
end

function mod.get_name(pid)
	if (cons[pid]) then
		-- TODO: should this be configurable? should i make lots of random trash configurable?
		return "console";
	end
	return next_call("get_name", mod.get_name)(pid);
end

local function handle_line(pid, line)
	-- TODO: handle '/' at start?
	if (#line ~= 0) then
		handle_command(pid, line, true);
	end
end

-- TODO: let fakepid login somehow. . . maybe also add f-orce-login cmd to login to any account with no password?
-- TODO: poll???
function mod.after.tick()
	-- TODO: convert a lot of ipairs into pairs, x/y into <something useful>?
	while (true) do
		local newcon = un.accept_unix_sock(sock);
		if (newcon == -1) then
			break;
		end

		local conpid = new_fakepid();
		-- TODO: optional perms? make sock_console cap?
		if (grant_cap) then
			grant_cap(conpid, "all");
		end

		cons[conpid] = {fd=newcon, buf=buffer.new(), unsent=buffer.new()};
	end

	for pid,con in pairs(cons) do
		flush(con);
		::start::
		local recvlen = un.recv_unix_sock(con.fd, buf, buflen);
		if (recvlen == 0) then
			rmcon(pid);
			goto continue;
		elseif (recvlen == -1) then
			goto continue;
		end

		-- TODO: handle *actual* errors
		con.buf:putcdata(buf, recvlen);

		if (recvlen == buflen) then
			-- Add some more junk to the buffer if possible
			goto start;
		end

		-- TODO: handle embedded NULLs
		local ptr, len = con.buf:ref();
		local found = ffi.C.memchr(ptr, string.byte("\n"), len);

		if (found ~= nil) then
			handle_line(pid, con.buf:get(ffi.cast("unsigned char *", found)-ptr));
			con.buf:skip(1);
		end

		::continue::
	end
end

return mod;
