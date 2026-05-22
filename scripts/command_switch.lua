-- command_switch.lua -- Shuffle other players' teams around

local invalid_team_msg = {
	en="team should be one of 1, 2, 256, -1, spec, spectator, %(firstteam), %(secondteam)."
};

-- TODO: switch from spec -> red, you die in red spawn, -> blue, you are still in red but will switch to blue on next spawn
-- TODO: /admin
-- TODO: respawn time outside of core
local teammap = {["1"]=1, ["2"]=2, ["256"]=SPECTATOR, ["-1"]=SPECTATOR, spec=SPECTATOR, spectator=SPECTATOR};
local nextmap = {2, 1, [SPECTATOR]=1};
local cmd = {name="switch", caps="switch", fakepid=true, usage="[player] [team]", desc="Move a player (or you) to a different team."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 2);
	local who = get_arg_pid_opt("player", pid, cmd, argv[1]) or pid;
	local team = argv[2];
	local teamid;

	if (team) then
		local namemap = {[string.lower(get_team_name(1))]=1, [string.lower(get_team_name(2))]=2};
		teamid = teammap[string.lower(team)];
		if (teamid == nil) then
			teamid = namemap[string.lower(team)];
		end
		if (teamid == nil) then
			send_usage(pid, cmd);
			l10n_send_chat(pid, invalid_team_msg, {firstteam=string.lower(get_team_name(1)), secondteam=string.lower(get_team_name(2))});
			return;
		end
	else
		teamid = nextmap[get_team(who)];
	end

	-- TODO: make dedicated switch func which is called by on_switch, and use that here
	on_switch(who, teamid, get_next_gun(who));
end
register_command(cmd, mod);

return {};
