-- command_kill.lua -- Death by grenade with extra steps

local cmd = {name="kill", usage="[player]", desc="Commit suicide."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local tokill = pid;

	-- TODO: check caps or make separate 1-arg /kill
	if (argv[1]) then
		tokill = get_arg_pid("player", pid, cmd, argv[1]);
	end

	if (is_alive(tokill)) then
		kill(tokill, 0, tokill);
	end
end
register_command(cmd);
