-- command_kill.lua -- Death by grenade with extra steps
local mod = init_mod();

local no_killing_ghosts_msg = {
	en="You can't kill yourself unless you're in-game."
};

-- TODO: you need to login to kill others?
local need_cap_msg = {
	en="You need the %(cap) capability to kill others."
};

-- TODO: hide from fakepid if not has_cap(pid, "kill")?
local cmd = {name="kill", fakepid=true, usage="[player]", desc="Kill another player or yourself."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local tokill = pid;

	-- TODO: check caps or make separate 1-arg /kill
	if (argv[1]) then
		if (not has_cap(pid, "kill")) then
			l10n_send_chat(pid, need_cap_msg, {cap="kill"});
			return;
		end
		tokill = get_arg_pid("player", pid, cmd, argv[1]);
	end

	if (is_fakepid(tokill)) then
		-- player was not specified and running pid is a fakepid, that's illegal
		send_usage(pid, cmd);
		l10n_send_chat(pid, no_killing_ghosts_msg);
		return;
	end

	if (is_alive(tokill)) then
		kill(tokill, 0, tokill);
	end
end
register_command(cmd, mod);

return mod;
