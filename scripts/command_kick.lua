-- command_kick.lua -- Remove a silly player.
local mod = init_mod();

local kicked_msg = {
	en="%(name) was kicked"
}

local cmd = {name="kick", caps="kick", fakepid=true, desc="player", usage="Remove a silly player."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local who = get_arg_pid("player", pid, cmd, argv[1]);

	-- TODO: work around notafile's generous contribution to betterspades
	-- TODO: it spread to ivspades too
	l10n_send_chat(PID_BROADCAST, kicked_msg, {name=get_name(who)});
	disconnect(who, 2);
end
register_command(cmd, mod);

return mod;
