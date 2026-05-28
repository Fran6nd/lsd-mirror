-- babel_unlocks.lua -- Give players the heaven cap after placing a kiloblock, and apoc after 5 captures
local mod = init_mod();
local blocks = pid_joined2_table(0);
local streak = pid_spawn_table(0);

-- apoc_streak borrowed from apoc.lua
getcfg("babel_unlocks_heaven_blocks", 1000);
getcfg("babel_unlocks_zones", {
	{
		{{x=128, y=256-babel_height/2}, {x=255-babel_width/2+20, y=255+babel_height/2}}
	},
	{
		{{x=256+babel_width/2-20, y=256-babel_height/2}, {x=383, y=255+babel_height/2}}
	}
});

local unlocked_msg = {
	en="%(name) has placed %(blocks) blocks, /heaven unlocked!"
};

-- TODO: handle "Deuce has placed 1 tower blocks."
local blocks_msg = {
	en="%(name) has placed %(blocks) tower blocks."
};

local fakepid_msg = {
	en="Congratulations on placing 0 blocks today!"
};

local apoc_not_unlocked_msg = {
	en="You need a %(needstreak) killstreak and 1 capture to use apoc! Current streak: %(curstreak)"
};

local apoc_streak_msg = {
	en="You have a %(streak) streak! Capture an intel for /apoc."
};

local cmd = {name="blocks", fakepid=true, usage="[player]", desc="Print the number of blocks placed by another player or yourself."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local player = get_arg_pid_opt("player", pid, cmd, argv[1]) or pid;

	if (is_fakepid(player)) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, fakepid_msg);
		return;
	end

	l10n_send_chat(pid, blocks_msg, {name=get_name(player), blocks=blocks[player]});
end
register_command(cmd, mod);

local function within(point, start, endp)
	return point >= start and point <= endp;
end

local function within_xy(pos, start, endp)
	return within(pos.x, start.x, endp.x) and within(pos.y, start.y, endp.y);
end

local function in_build_zone(pid, pos)
	local team = get_team(pid);

	for _,zone in ipairs(babel_unlocks_zones[team]) do
		if (within_xy(pos, zone[1], zone[2])) then
			return true;
		end
	end

	return false;
end

local function check_blocks_placed(pid)
	if (not has_cap(pid, "heaven") and blocks[pid] >= babel_unlocks_heaven_blocks) then
		l10n_send_chat(PID_BROADCAST, unlocked_msg, {name=get_name(pid), blocks=babel_heaven_unlock_blocks});
		grant_cap(pid, "heaven");
	end
end

function mod.after.on_block_action(pid, pos, type)
	if (type == 0 and in_build_zone(pid, pos)) then
		blocks[pid] = blocks[pid] + 1;
	end

	check_blocks_placed(pid);
end

function mod.before.on_block_line(pid, startp, endp)
	for pos in iter_block_line(startp, endp) do
		if (not is_solid(pos) and in_build_zone(pid, pos)) then
			blocks[pid] = blocks[pid] + 1;
		end
	end

	check_blocks_placed(pid);
end

function mod.after.kill(pid, type, killer)
	-- This also prevents /kill from increasing streak.
	if (get_team(pid) ~= get_team(killer)) then
		streak[killer] = streak[killer] + 1;
		if (streak[killer] == apoc_streak) then
			l10n_send_chat(killer, apoc_streak_msg, {streak=apoc_streak});
		end
	end
end

function mod.after.capture_intel(pid)
	if (apoc_grant == nil) then
		return;
	end

	if (streak[pid] >= apoc_streak) then
		streak[pid] = streak[pid] - apoc_streak;
		apoc_grant(pid);
	end

	if (get_team_score(get_team(pid)) % 5 == 0) then
		local maxblocks = blocks[pid];
		local maxi = pid;

		for i,blockcount in pairs(blocks) do
			if (blockcount > maxblocks) then
				maxi = i;
				maxblocks = blockcount;
			end
		end

		apoc_grant(maxi);
	end
end

function mod.send_apoc_not_unlocked_msg(pid)
	l10n_send_chat(pid, apoc_not_unlocked_msg, {needstreak=apoc_streak, curstreak=streak[pid]});
end

function mod.try_apoc_grant(pid)
end

return mod;
