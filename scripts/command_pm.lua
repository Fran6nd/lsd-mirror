-- command_pm.lua -- Pseudoprivately harass someone with mean messages
local mod = init_mod();
require "lib_l10n";

-- IVspades depends on the message being prefixed with "PM from "
-- Do with that information what you will.
local pm_msg = {
	en="PM from %(name): %(msg)"
};

-- TODO: don't argparse after the player arg
local cmd = {name="pm", fakepid=true, usage="player msg...", desc="Send a pseudoprivate message to someone."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv >= 2);
	local player = get_arg_pid("player", pid, cmd, argv[1]);

	if (not has_cap(pid, "badcap:nopm") and (cmd_ignore_ignored == nil or not cmd_ignore_ignored[player][pid])) then
		l10n_send_chat(player, pm_msg, {name=get_name(pid), msg=table.concat(argv, " ", 2)});
	end
end
register_command(cmd);
