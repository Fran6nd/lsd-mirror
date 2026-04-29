-- command_caps.lua -- Grant and revoke arbitrary caps

local cmd = {name="grantcap", caps="caps", fakepid=true, usage="player caps...", desc="Give a player arbitrary caps."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 2);
	player = get_arg_pid("player", pid, cmd, argv[1]);

	for x,y in ipairs(argv) do
		grant_cap(player, y);
	end
end
register_command(cmd);

local cmd = {name="dropcap", caps="caps", fakepid=true, usage="player caps...", desc="Remove arbitrary caps from a player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 2);
	player = get_arg_pid("player", pid, cmd, argv[1]);

	for x,y in ipairs(argv) do
		drop_cap(player, y);
	end
end
register_command(cmd);

return {};
