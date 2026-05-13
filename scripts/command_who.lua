-- command_who.lua -- Hello, is there anybody in there?

local cmd = {name={"who", "lscon", "listconnections"}, fakepid=true, usage="msg", desc="Print all joined players."};
function cmd.func(pid, argv, msg)
	cmd_assert(pid, cmd, #argv == 0);

	for i in piditer(PID_BROADCAST) do
		if (is_joined(i)) then
			server_msg(pid, "#"..i..": "..get_name(i));
		end
	end
end
register_command(cmd);

return {};
