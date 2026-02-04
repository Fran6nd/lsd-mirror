-- lib_sock.lua -- TODO: not a lib, just a dep TODO: mention the fakepids?
local mod = init_mod();
local ffi = require("ffi");
local buffer = require("string.buffer");

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
-- TODO: if i can remove this on unload, should i?
local buflen = 1024;
local buf = ffi.new("char[?]", buflen);
local socks;

local function on_disconnect(sock, pid)
	free_fakepid(pid);
end

-- Returns the key to be used for the sock, which is called pid since
-- all the default calls except on_recv assume it's a pid
local function on_connect(sock, con)
	local pid = new_fakepid();

	-- TODO: optional perms? make sock_console cap?
	if (grant_cap) then
		grant_cap(pid, "all");
	end

	return pid;
end

-- Default line handler runs a command
local function on_line(sock, pid, line)
	if (#line ~= 0) then
		log("lib_sock: %s (#%u): /%s", get_name(pid), pid, line);
		handle_command(pid, line, true);
	end
end

-- Default recv handler buffers things by line
local function on_recv(sock, pid)
	local con = sock.cons[pid];

	-- TODO: handle embedded NULLs
	local ptr, len = con.buf:ref();
	local found = ffi.C.memchr(ptr, string.byte("\n"), len);

	if (found ~= nil) then
		sock.on_line(sock, pid, con.buf:get(ffi.cast("unsigned char *", found)-ptr));
		con.buf:skip(1);
	end
end

function sock_new_unix(file)
	-- If file already exists make an attempt to remove it
	ffi.C.unlink(file);
	fd = un.create_unix_sock(file);
	if (fd == -1) then
		-- TODO: return errno?
		error("socket: <some error>");
	end

	local sock = {fd=fd, path=path, cons={}, on_recv=on_recv, on_line=on_line, on_connect=on_connect, on_disconnect=on_disconnect};
	socks[sock] = true;
	return sock;
end

local function rmcon(sock, cid)
	sock.on_disconnect(sock, cid);
	-- TODO: does closing the main socket already cover this in on_unload's case?
	un.close_unix_sock(sock.cons[cid].fd);
	-- TODO: you can surely optimize this better
	sock.cons[cid] = nil;
end

function sock_close(sock)
	for cid,con in pairs(sock.cons) do
		rmcon(sock, cid);
	end

	un.close_unix_sock(sock.fd);

	-- Hopefully this removes sock_console
	if (sock.path) then
		ffi.C.unlink(sock.path);
	end

	socks[sock] = nil;
end

local function flush(con)
	local ptr, len = con.unsent:ref();
	local sentlen = un.send_unix_sock(con.fd, ptr, len);
	if (sentlen ~= -1) then
		con.unsent:skip(sentlen);
	end
end

function sock_send_con(con, text)
	con.unsent:put(text);
	flush(con);
end

function sock_send_broadcast(sock, text)
	for cid,con in pairs(sock.cons) do
		con.unsent:put(text);
		flush(con);
	end
end

function mod.on_load()
	socks = {};
end

function mod.on_unload()
	for sock,_ in pairs(socks) do
		sock_close(sock);
	end

	socks = nil;
	-- TODO: cleanup random buffers?
end

-- TODO: poll???
function mod.after.tick()
	for sock,_ in pairs(socks) do
		-- TODO: convert a lot of ipairs into pairs, x/y into <something useful>?
		while (true) do
			local newcon = un.accept_unix_sock(sock.fd);
			if (newcon == -1) then
				break;
			end

			local con = {fd=newcon, buf=buffer.new(), unsent=buffer.new()};
			local cid = sock.on_connect(sock, con);
			sock.cons[cid] = con;
		end

		for cid,con in pairs(sock.cons) do
			flush(con);

			::start::
			local recvlen = un.recv_unix_sock(con.fd, buf, buflen);

			if (recvlen == 0) then
				rmcon(sock, cid);
			elseif (recvlen ~= -1) then
				-- TODO: handle *actual* errors
				con.buf:putcdata(buf, recvlen);

				if (recvlen == buflen) then
					-- Add some more junk to the buffer if possible
					goto start;
				end

				sock.on_recv(sock, cid);
			end
		end
	end
end

return mod;
