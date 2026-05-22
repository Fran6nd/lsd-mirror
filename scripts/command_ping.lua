-- command_ping.lua -- Check to see just how laggy you are

local ping_msg = {
	en="%(name)'s round-trip time is %(rtt) ms."
}

local fakepid_msg = {
	en="You can only check your own round-trip time while in-game."
};

-- TODO: function to get optional pid or otherwise use me
local cmd = {name="ping", fakepid=true, usage="[player]", desc="Print a player's (or your) round-trip time."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local who = pid;

	if (argv[1]) then
		who = get_arg_pid("player", pid, cmd, argv[1]);
	end

	if (is_fakepid(who)) then
		-- player was not specified and running pid is a fakepid, that's illegal
		-- TODO: avoid copy/pasting this to every func with an optional player arg
		send_usage(pid, cmd);
		l10n_send_chat(pid, fakepid_msg);
		return;
	end

	l10n_send_chat(pid, ping_msg, {name=get_name(who), rtt=get_round_trip_time(who)});
end
register_command(cmd, mod);

return {};
