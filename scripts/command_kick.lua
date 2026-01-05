-- command_kick.lua -- Remove a silly player.

local cmd = {name="kick", caps="kick", desc="player", usage="Remove a silly player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local who = get_arg_pid("player", pid, cmd, argv[1]);

	-- TODO: work around notafile's generous contribution to betterspades
	-- TODO: it spread to ivspades too
	-- TODO: add l10n send_chat
	send_chat(PID_BROADCAST, get_name(who).." was kicked", 2, 0);
	disconnect(who, 2);
end
register_command(cmd);
