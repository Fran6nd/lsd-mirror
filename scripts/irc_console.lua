-- irc_console.lua -- Run commands from IRC
local buffer = require("string.buffer");
local mod = init_mod();
local sock;

getcfg("irc_console_addr", "127.0.0.1");
getcfg("irc_console_port", 6667);
getcfg("irc_console_nick", "server");
getcfg("irc_console_chan", "#console");
getcfg("irc_console_pfx", "server!");
getcfg("irc_console_name", "@console");

local function finish_arg(argv, argc, str)
	if     (argc == 0 and argv.src == nil and argv.tags == nil and string.sub(str, 1, 1) == '@') then
                argv.tags = string.sub(str, 2);
	elseif (argc == 0 and argv.src == nil and string.sub(str, 1, 1) == ':') then
                argv.src = string.sub(str, 2);
	else
		if (argc == 0) then
			str = string.upper(str);
		end

                argv[argc] = str;
                argc = argc + 1;
	end

	return argc;
end

local function split_to_table(msg)
        local buf = buffer.new(#msg);
        local squish = false;
        local argv = {};
        local argc = 0;

        for i=1,#msg do
                local chr = string.sub(msg, i, i);

                if (chr ~= ' ') then
			if (argc > 0 and squish and chr == ':') then
				argv[argc] = string.sub(msg, i+1);
				return argv;
			end

                        buf:put(chr);
                        squish = false;
                elseif (not squish) then
			argc = finish_arg(argv, argc, buf:get());
                        squish = true;
                end
        end

        if (#buf) then
		argc = finish_arg(argv, argc, buf:get());
        end

        return argv;
end

local function cmd_tostr(cmd)
	local str = cmd[0];

	for i=1,#cmd do
		if (i ~= #cmd) then
			str = str.." "..cmd[i];
		else
			str = str.." :"..cmd[i];
		end
	end

	return str.."\r\n";
end

local cur_fakepid;
local function run_cmd(line)
	cur_fakepid = new_fakepid();

	-- TODO: handle this a little better lol
	commands.forcelogin.func(cur_fakepid, {[0]="forcelogin", "notaburner"});
        log("irc_console: %s (#%u): /%s", get_name(cur_fakepid), cur_fakepid, line);
        handle_command(cur_fakepid, line, true);

	free_fakepid(cur_fakepid);
	cur_fakepid = nil;
end

local function on_line(sock, pid, line)
	line = string.gsub(line, "\r$", "", 1);
	--log("%s", line);

	local cmd = split_to_table(line);
	--log("%s", fmtval(cmd));

	if (cmd[0] == "PING") then
		cmd[0] = "PONG";

		sock_send_broadcast(sock, cmd_tostr(cmd));
	elseif (cmd[0] == "PRIVMSG") then
		--log("PRIVMSG from %s to %s: %s", cmd.src, cmd[1], cmd[2]);
		-- TODO: should I document that the prefix is a pattern?
		local nopfx = string.gsub(cmd[2], "^"..irc_console_pfx, "", 1);

		if (nopfx ~= cmd[2]) then
			run_cmd(nopfx);
		end
	end
end

local function on_connect()
	return 1;
end

local function null()
end

function mod.on_load()
        sock = nil;
        sock = sock_new_tcp_client(irc_console_addr, irc_console_port, {on_line=on_line, on_connect=on_connect, on_disconnect=null});
        sock_send_broadcast(sock, "NICK "..irc_console_nick.."\r\nUSER "..irc_console_nick.." 0 * "..irc_console_nick.."\r\nJOIN "..irc_console_chan.."\r\n");
end

function mod.on_unload()
        if (sock) then
                sock_close(sock);
        end
end

function mod.early.log(fmt, ...)
        for pid,con in pairs(sock.cons) do
		local msg = string.format(fmt, ...);

		for line in string.gmatch(msg, "[^\n]+") do
			sock_send_broadcast(sock, cmd_tostr({[0]="PRIVMSG", irc_console_chan, line}));
		end
        end
        mod.early.next.log(fmt, ...);
end

function mod.early.send_chat(pid, msg, type, from)
        -- TODO: one conpid per connection? OPTIONAL?
        if (pid == cur_fakepid) then
		for line in string.gmatch(msg, "[^\n]+") do
			sock_send_broadcast(sock, cmd_tostr({[0]="PRIVMSG", irc_console_chan, line}));
		end
                return;
        end
        mod.early.next.send_chat(pid, msg, type, from);
end

function mod.early.get_name(pid)
        if (pid == cur_fakepid) then
                return irc_console_name;
        end
        return mod.early.next.get_name(pid);
end

return mod;
