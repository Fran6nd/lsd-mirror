-- command_tp.lua -- Teleport people around.
local mod = init_mod();

-- TODO: for crap-positiondata, should it just ignore moving too far? legitimate players'll get synced correctly anyway, so it's counter-productive to teleport them once more -- illegitimate ones get to have fun

local fakepid_msg = {
	en="You have gone nowhere successfully."
};

-- TODO: what about caps for just tp-style and not tp2-style?
local cmd = {name="tp", caps="tp", fakepid=true, usage="[moveplayer] toplayer", desc="Teleport one player (or you) to a different player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1 or #argv == 2);

	local from;
	local to;
	if (#argv == 2) then
		from = get_arg_pid("moveplayer", pid, cmd, argv[1]);
		to = get_arg_pid("toplayer", pid, cmd, argv[2]);
	elseif (not is_fakepid(pid)) then
		from = pid;
		to = get_arg_pid("toplayer", pid, cmd, argv[1]);
	else
		send_usage(pid, cmd);
		l10n_send_chat(pid, fakepid_msg);
		return;
	end


	set_position(from, get_position(to));
end
register_command(cmd, mod);

-- TODO: do i need to keep an upvalue for mod?
return mod;
