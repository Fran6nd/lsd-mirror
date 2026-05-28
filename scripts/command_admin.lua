-- command_admin.lua -- #32 is hacking!!!
local mod = init_mod();

local function fmt_name(pid)
	if (auth_users and auth_users[pid] and not is_fakepid(pid)) then
		return get_name(pid).." (@"..auth_users[pid]..")";
	end

	return get_name(pid);
end

local cmd = {name="admin", fakepid=true, usage="msg", desc="Send a message to staff. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	if (has_cap(pid, "badcap:noadmin")) then
		return;
	end

	local msg = string.format("admin: %s (#%u): %s", fmt_name(pid), pid, string.sub(msg, 7));
	log("%s", msg);

	-- TODO: fakepid? (need to kill fakepid and implement pluggable connections)
	for i in piditer(PID_BROADCAST) do
		if (has_cap(i, "admin_chat")) then
			server_msg(i, msg);
		end
	end
end
register_command(cmd, mod);

local cmd = {name={"adminchat", "sadmin"}, caps="admin_chat", fakepid=true, usage="msg", desc="Send a message to staff, but only staff can use it. Does not parse arguments."};
function cmd.func(pid, argv, msg)
	local msg = string.format("adminchat: %s (#%u): %s", fmt_name(pid), pid, string.sub(msg, #argv[0]+2));
	log("%s", msg);

	for i in piditer(PID_BROADCAST) do
		if (has_cap(i, "admin_chat")) then
			server_msg(i, msg);
		end
	end
end
register_command(cmd, mod);

return mod;
