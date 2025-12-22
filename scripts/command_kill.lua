-- command_kill.lua -- Death by grenade with extra steps

local cmd = {name="kill"};
function cmd.func(pid, argv)
	if (is_alive(pid)) then
		kill(pid, 0, pid);
	end
end

register_command(cmd);
