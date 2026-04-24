-- command_switch.lua -- Shuffle other players' teams around
require "lib_l10n";

local invalid_team_msg = {
	en="team should be one of 0, 1, 255, -1, spec, spectator, %(firstteam), %(secondteam)."
};

-- TODO: switch from spec -> red, you die in red spawn, -> blue, you are still in red but will switch to blue on next spawn
-- TODO: /kick
-- TODO: /admin
-- TODO: respawn time outside of core
-- TODO: retarded pyspades uses 1,2 -> 0,1
local teammap = {["0"]=0, ["1"]=1, ["255"]=255, ["-1"]=255, spec=255, spectator=255};
local cmd = {name="switch", caps="switch", fakepid=true, usage="[player] [team]", desc="Move a player (or you) to a different team."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 2);
	local who = get_arg_pid_opt("player", pid, cmd, argv[1]) or pid;
	local team = argv[2];
	local teamid;

	if (team) then
		local namemap = {[string.lower(get_team_name(0))]=0, [string.lower(get_team_name(1))]=1};
		teamid = teammap[string.lower(team)];
		if (teamid == nil) then
			teamid = namemap[string.lower(team)];
		end
		if (teamid == nil) then
			send_usage(pid, cmd);
			l10n_send_chat(pid, invalid_team_msg, {firstteam=string.lower(get_team_name(0)), secondteam=string.lower(get_team_name(0))});
			return;
		end
	else
		teamid = (get_team(who) + 1) % 2;
	end

	-- TODO: make dedicated switch func which is called by on_switch, and use that here
	on_switch(who, teamid, get_next_weapon(who));
end
register_command(cmd);

return {};
