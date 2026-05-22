-- lib_sock.lua -- TODO: not a lib, just a dep TODO: mention the fakepids?
local mod = init_mod();
local ffi = require("ffi");
local buffer = require("string.buffer");

-- TODO: seccomp away all but accept() (except maybe for masterlist) after config loaded?
pcall(function()ffi.cdef[[
int create_unix_sock(const char *path);
int create_tcp_sock(const char *listenaddr, const char *port);
int create_tcp_client_sock(const char *connectaddr, const char *port);
int accept_sock(int fd);
void close_sock(int fd);
ssize_t send_sock(int fd, const char *buf, size_t len);
ssize_t recv_sock(int fd, char *buf, size_t len);
void *memchr(const void *s, int c, size_t n);
int unlink(const char *path);

struct WS_Headers {
	unsigned gotFlags;
	char keyBuf[24];
	char hdrBuf[46];
	uint8_t hdrBufLen;
};
struct WS_Frame {
	uint64_t dataLen;
	size_t contHdrLen;
	uint8_t mask[4];
	unsigned opcode;
	int final;
	int masked;

	uint8_t tmpBuf[12];
	uint8_t tmpBufLen;
};
struct WS_Event {
	unsigned type;
	unsigned flags;
	void *recvData;
	size_t recvLen;
};
int websockets_read_handshake_hdrs(const char *in, size_t size, struct WS_Headers *hdr);
size_t websockets_fill_handshake_buf(char *out, struct WS_Headers *hdr);
size_t websockets_consume(struct WS_Frame *frame, struct WS_Event *event, uint8_t *in, size_t size);
size_t websockets_create_frame(uint8_t *buf, uint32_t opcode, int final, uint64_t size);
size_t websockets_create_close_frame(uint8_t *buf, uint16_t code);

int close(int fildes);
int fcntl(int fildes, int cmd, ...);
]];end)

local WS_OPCODE_CONT =  0
local WS_OPCODE_TEXT =  1
local WS_OPCODE_BLOB =  2

local WS_OPCODE_CLOSE = 8
local WS_OPCODE_PING =  9
local WS_OPCODE_PONG =  10

local WS_OPCODE_ERR =   255

local WS_EVENT_RECV =  1
local WS_EVENT_CLOSE = 2
local WS_EVENT_PING =  4
local WS_EVENT_ERR =   8

local WS_EVENT_FLAG_RECV_START = 1
local WS_EVENT_FLAG_RECV_END = 2

local un = ffi.load("./exec/libunixsock.so");
-- TODO: nuke pid with weak table
-- TODO: if i can remove this on unload, should i?
local buflen = 1024;
local buf = ffi.new("char[?]", buflen);
local socks;

local function flush(con)
	local ptr, len = con.unsent:ref();
	local sentlen = un.send_sock(con.fd, ptr, len);
	if (sentlen ~= -1) then
		con.unsent:skip(sentlen);
	end
end

function rmcon(sock, cid)
	sock.on_disconnect(sock, cid);
	-- TODO: does closing the main socket already cover this in on_unload's case?
	un.close_sock(sock.cons[cid].fd);
	-- TODO: you can surely optimize this better
	sock.cons[cid] = nil;

	if (sock.client) then
		socks[sock] = nil;
	end
end

function websock_invalid_hdrs(sock, cid)
	local con = sock.cons[cid];
	con.wsclose = true;
	con.unsent:putcdata(buf, un.websockets_fill_handshake_buf(buf, con.wshdr));
	flush(con);
	rmcon(sock, cid);
end

-- TODO: add shut_rd to each con, shutdown(SHUT_RD) when true, rmcon on #unsent == 0, remove from parent sock and put into local table for dying cons
-- (or just kill it)
-- Depends on con.wshdr being an initialized struct WS_Headers
local function websock_read_hdrs(sock, cid)
	local con = sock.cons[cid];
	local ptr, len = con.buf:ref();
	local ret = un.websockets_read_handshake_hdrs(ptr, len, con.wshdr);
	con.buf:reset();
	if (ret ~= 0) then
		if (ret == -1) then
			sock.ws_invalid_hdrs(sock, cid, con.wshdr.gotFlags);
		else
			con.unsent:putcdata(buf, un.websockets_fill_handshake_buf(buf, con.wshdr));
			con.unsent:put(con.wsunsent);
			flush(con);
			sock.on_ws_connect(sock, cid);
		end
		con.wshdr = nil;
		con.wsunsent = nil;
	end
	return ret;
end

-- TODO: should wsevent be moved into websock_read?
local wsevent = ffi.new("struct WS_Event");
local function websock_read(sock, cid)
	local con = sock.cons[cid];
	while (#con.buf > 0) do
		local ptr, len = con.buf:ref();
		con.buf:skip(un.websockets_consume(con.wsframe, wsevent, ptr, len));

		if (wsevent.type == WS_EVENT_RECV) then
			con.wsbuf:putcdata(wsevent.recvData, wsevent.recvLen);
			if (bit.band(wsevent.flags, WS_EVENT_FLAG_RECV_END) ~= 0) then
				sock.on_ws_recv(sock, cid);
				con.wsbuf:reset();
			end
		elseif (wsevent.type == WS_EVENT_CLOSE) then
			un.websockets_create_frame(buf, WS_OPCODE_CLOSE, 1, wsevent.recvLen >= 2 and 2 or 0);
			ffi.copy(buf+2, wsevent.recvData, wsevent.recvLen >= 2 and 2 or 0);
			con.unsent:putcdata(buf, wsevent.recvLen >= 2 and 4 or 2);
			flush(con);
			-- TODO: does this gracefully disconnect? you may have to shutdown(SHUT_RD) instead and close when all of unsent isn't
			con.wsclose = true;
			rmcon(sock, cid);
			return;
		elseif (wsevent.type == WS_EVENT_PING) then
			websockets_create_frame(buf, WS_OPCODE_PING, 1, wsevent.recvLen);
			memcpy(buf+2, wsevent.recvData, wsevent.recvLen);
			con.unsent:putcdata(buf, 2+recvLen);
			flush(con);
		elseif (wsevent.type == WS_EVENT_ERR) then
			websock_disconnect(sock, cid, 1002);
			return;
		end
	end
end

function websock_send_con(con, text)
	if (not con.wsclose) then
		local unsentbuf = con.unsent;

		if (con.wshdr) then
			unsentbuf = con.wsunsent;
		end

		unsentbuf:putcdata(buf, un.websockets_create_frame(buf, WS_OPCODE_TEXT, 1, #text));
		unsentbuf:put(text);
		if (not con.wshdr) then
			flush(con);
		end
	end
end

function websock_disconnect(sock, cid, code)
	local con = sock.cons[cid];
	un.websockets_create_close_frame(buf, code and code or 0);
	-- TODO: is con nil here?
	con.unsent:putcdata(buf, code and 4 or 2);
	flush(con);
	-- TODO: does this gracefully disconnect? you may have to shutdown(SHUT_RD) instead and close when all of unsent isn't
	-- TODO: should this gracefully disconnect?
	con.wsclose = true;
	rmcon(sock, cid);
end

function websock_init_con(con)
	con.wsbuf = buffer.new();
	con.wsunsent = buffer.new();
	con.wsclose = false;
	con.wshdr = ffi.new("struct WS_Headers");
	con.wsframe = ffi.new("struct WS_Frame");
	con.wsframe.final = 1;
end

function websock_recv(sock, cid)
	local con = sock.cons[cid];

	if (not con.wsclose) then
		if (con.wshdr) then
			websock_read_hdrs(sock, cid);
		else
			websock_read(sock, cid);
		end
	end
end

local function on_disconnect(sock, pid)
	free_fakepid(pid);
end

-- Returns the key to be used for the sock, which is called pid since
-- all the default calls except on_recv assume it's a pid
local function on_connect(sock, con)
	return new_fakepid();
end

local function after_connect(sock, pid)
	on_fakepid_connect(pid);

	-- TODO: optional perms? make sock_console cap?
	if (grant_cap) then
		grant_cap(pid, "all");
	end
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
	while (true) do
		local ptr, len = con.buf:ref();
		local found = ffi.C.memchr(ptr, string.byte("\n"), len);

		if (found == nil) then
			break;
		end
			sock.on_line(sock, pid, con.buf:get(ffi.cast("unsigned char *", found)-ptr));
			con.buf:skip(1);
	end
end

-- TODO: dedup
function sock_new_unix(file)
	-- If file already exists make an attempt to remove it
	ffi.C.unlink(file);
	fd = un.create_unix_sock(file);
	if (fd == -1) then
		-- TODO: return errno?
		error("socket: <some error>");
	end

	local sock = {fd=fd, path=path, cons={}, on_recv=on_recv, on_line=on_line, on_connect=on_connect, after_connect=after_connect, on_disconnect=on_disconnect};
	socks[sock] = true;
	return sock;
end

function sock_new_tcp(listenaddr, port)
	fd = un.create_tcp_sock(listenaddr, tostring(port));
	if (fd == -1) then
		-- TODO: return errno?
		error("socket: <some error>");
	end

	local sock = {fd=fd, cons={}, on_recv=on_recv, on_line=on_line, on_connect=on_connect, after_connect=after_connect, on_disconnect=on_disconnect};
	socks[sock] = true;
	return sock;
end

function sock_new_tcp_client(connectaddr, port, funcs)
	funcs = funcs or {};
	fd = un.create_tcp_client_sock(connectaddr, tostring(port));
	if (fd == -1) then
		-- TODO: return errno?
		error("socket: <some error>");
	end

	local sock = {client=true, fd=fd, cons={}, on_recv=funcs.on_recv or on_recv, on_line=funcs.on_line or on_line, on_connect=funcs.on_connect or on_connect, after_connect=funcs.after_connect or after_connect, on_disconnect=funcs.on_disconnect or on_disconnect};
	socks[sock] = true;

	local con = {fd=fd, buf=buffer.new(), unsent=buffer.new()};
	local cid = sock.on_connect(sock, con);
	sock.cons[cid] = con;
	sock.after_connect(sock, cid);

	return sock;
end

function sock_close(sock)
	for cid,con in pairs(sock.cons) do
		local confd = sock.cons[cid].fd;

		rmcon(sock, cid);

		if (confd == sock.fd) then
			return;
		end
	end

	un.close_sock(sock.fd);

	-- Hopefully this removes sock_console
	if (sock.path) then
		ffi.C.unlink(sock.path);
	end

	socks[sock] = nil;
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
		if (not sock.client) then
			while (true) do
				local newcon = un.accept_sock(sock.fd);
				if (newcon == -1) then
					break;
				end

				local con = {fd=newcon, buf=buffer.new(), unsent=buffer.new()};
				local cid = sock.on_connect(sock, con);
				sock.cons[cid] = con;
				sock.after_connect(sock, cid);
			end
		end

		for cid,con in pairs(sock.cons) do
			flush(con);

			::start::
			local recvlen = un.recv_sock(con.fd, buf, buflen);

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
