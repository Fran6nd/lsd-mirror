-- command_ratio.lua -- Display number of shots missed
local mod = init_mod();
-- TODO: use joined_table?
-- kills is the number of players killed by this one; deaths is the number of times this one died
local ratio = pid_connected_table(function() return {kills=0, deaths=0} end);

local spooky_ratio_msg = {
	en="You haven't even made one kill. No deaths though."
};

local ratio_msg = {
	en="%(name): %(kills) kills, %(deaths) deaths, %(ratio) kills per life"
}

-- TODO: save kill-life ratio at each death and display that?
local cmd = {name="ratio", fakepid=true, usage="[player]", desc="Print the kill-life ratio of another player or yourself."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local player = get_arg_pid_opt("player", pid, cmd, argv[1]) or pid;

	if (is_fakepid(player)) then
		-- TODO: deduplicate this across scripts
		send_usage(pid, cmd);
		l10n_send_chat(pid, spooky_ratio_msg);
		return;
	end

	ratio[player].name = get_name(player);
	ratio[player].ratio = string.format("%.2f", ratio[player].kills / (ratio[player].deaths + 1));
	l10n_send_chat(pid, ratio_msg, ratio[player]);
end
register_command(cmd);

function mod.after.kill(pid, type, killer)
	-- TODO: should killing teammates increase score? it does as of now
	-- TODO: is suicide a death? teamkill?
	if (type < 4 and pid ~= killer) then
		ratio[killer].kills = ratio[killer].kills + 1;
		ratio[pid].deaths = ratio[pid].deaths + 1;
	end
end

return mod;
