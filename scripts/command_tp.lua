-- command_tp.lua -- Teleport people around.

-- TODO: for crap-positiondata, should it just ignore moving too far? legitimate players'll get synced correctly anyway, so it's counter-productive to teleport them once more -- illegitimate ones get to have fun

-- TODO: what about caps for just tp-style and not tp2-style?
local cmd = {name="tp", caps="tp", usage="moveplayer toplayer", desc="Teleport one player to a different player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 2);
	local from = get_arg_pid("moveplayer", pid, cmd, argv[1]);
	local to = get_arg_pid("toplayer", pid, cmd, argv[2]);

	set_position(from, get_position(to));
end
register_command(cmd);
