-- team_balance.lua -- Attempt to balance quantity but fail to balance quality
require("lib_l10n");

local mod = init_mod();

getcfg("team_balance_max_diff", 2);

local unbalanced_join_msg = {
	en="%(team) is unbalanced; switched to other team"
};

local unbalanced_switch_msg = {
	en="%(team) is unbalanced; cannot switch"
};

local function count_players_on_teams(pid, team)
	local players = {[0]=0, [1]=0};
	for i in piditer(PID_BROADCAST) do
		local pteam = i == pid and team or get_team(i);
		players[pteam] = players[pteam] + 1;
	end

	return players;
end

function mod.on_join(pid, team, gun, name)
	if (team ~= SPECTATOR) then
		local players = count_players_on_teams(pid, team);

		if (players[team] - players[team == 1 and 0 or 1] > team_balance_max_diff) then
			l10n_send_chat(pid, unbalanced_join_msg, {team=get_team_name(team)});
			team = team == 1 and 0 or 1;
		end
	end

	return next_call("on_join", mod.on_join)(pid, team, gun, name);
end

function mod.on_switch(pid, team, gun)
	if (get_team(pid) ~= team and team ~= SPECTATOR) then
		local players = count_players_on_teams(pid, team);

		if (players[team] - players[team == 1 and 0 or 1] > team_balance_max_diff) then
			l10n_send_chat(pid, unbalanced_switch_msg, {team=get_team_name(team)});
			team = get_team(pid);

			-- TODO: still have to determine if buggerspades switch to same team/gun is a feature or bug
			if (gun == get_gun(pid)) then
				return;
			end
		end
	end

	return next_call("on_switch", mod.on_switch)(pid, team, gun);
end

return mod;
