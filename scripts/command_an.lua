-- command_an.lua -- That hit packet dumper with extra steps
local mod = init_mod();

local targets;
local revtargets;
local disabled_msg;

local function onclear_targets(pid)
	local target = targets[pid];

	if (target) then
		revtargets[target][pid] = nil;
	end
end

local function onclear_revtargets(pid)
	for i,_ in pairs(revtargets[pid]) do
		targets[i] = nil;
		l10n_send_chat(i, disabled_msg);
	end
end

local last_hit_time = pid_joined2_table(nil);
targets = pid_joined2_table(nil, onclear_targets);
revtargets = pid_joined2_table(function() return {}; end, onclear_revtargets);

-- Takes the place of %(iscrap) in the other messages if the packet was marked as crap.
local crap_msg = {
	en=", crap"
};

local torso_msg = {
	en="%(player) hit %(hitplayer) (torso, %(dist) blocks, %(delta) ms%(iscrap))"
};

local head_msg = {
	en="%(player) hit %(hitplayer)  (head, %(dist) blocks, %(delta) ms%(iscrap))"
};

local arms_msg = {
	en="%(player) hit %(hitplayer)  (arms, %(dist) blocks, %(delta) ms%(iscrap))"
};

local legs_msg = {
	en="%(player) hit %(hitplayer)  (legs, %(dist) blocks, %(delta) ms%(iscrap))"
};

local spade_msg = {
	en="%(player) hit %(hitplayer) (spade, %(dist) blocks, %(delta) ms%(iscrap))"
};

local crap_len_msg = {
	en="%(player) sent a hit packet with a crap length"
};

local crap_type_msg = {
	en="%(player) sent a hit packet with a crap type"
};

local crap_target_msg = {
	en="%(player) sent a hit packet with a crap target"
};

disabled_msg = {
	en="Stopped analyzing."
};

local an_msg = {
	en="Now analyzing %(player)."
};

local msgmap = {[0]=torso_msg, head_msg, arms_msg, legs_msg, spade_msg};

local cmd = {name={"an", "analyze"}, fakepid=true, usage="[player]", desc="Dump out whatever hit packets a player sends."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local player = get_arg_pid_opt("player", pid, cmd, argv[1]);

	if (targets[pid]) then
		revtargets[targets[pid]][pid] = nil;
		targets[pid] = nil;
	end

	if (player == nil) then
		l10n_send_chat(pid, disabled_msg);
		return;
	end

	targets[pid] = player;
	revtargets[player][pid] = true;
	l10n_send_chat(pid, an_msg, {player=get_name(player)});
end
register_command(cmd, mod);

--[[
struct PacketHit {
	uint8_t packetID = 5;
	uint8_t target;
	uint8_t type;
}
]]--

local function distance_2d(player1, player2)
	local p1 = get_position(player1);
	local p2 = get_position(player2);
	local diff = {x=p2.x-p1.x, y=p2.y-p1.y};

	return math.sqrt(diff.x*diff.x + diff.y*diff.y);
end

local function msg_revtargets(pid, ...)
	for i,_ in pairs(revtargets[pid]) do
		l10n_send_chat(i, ...);
	end
end

local function handle_packet(pid, data, iscrap)
	if (string.byte(data, 1) ~= 5) then
		return;
	end

	local now = get_time();

	if (last_hit_time[pid] == nil) then
		last_hit_time[pid] = 0/0;
	end

	local delta = math.floor((now - last_hit_time[pid]) * 1000);
	last_hit_time[pid] = now;

	if (#data ~= 3) then
		msg_revtargets(pid, crap_len_msg, {player=get_name(pid)});
		return;
	end

	local target = string.byte(data, 2);
	local type = string.byte(data, 3);

	if (type >= 5) then
		msg_revtargets(pid, crap_type_msg, {player=get_name(pid)});
		return;
	end

	if (target >= MAX_PLAYERS or not is_joined(target)) then
		msg_revtargets(pid, crap_target_msg, {player=get_name(pid)});
		return;
	end

	local dist = math.floor(distance_2d(pid, target) * 10) / 10;
	local iscrapmsg = iscrap and l10n_get_str_pid(pid, crap_msg) or "";

	msg_revtargets(pid, msgmap[type], {player=get_name(pid), hitplayer=get_name(target), dist=dist, delta=delta, iscrap=iscrapmsg});
end

function mod.after.on_sane_packet(pid, data)
	handle_packet(pid, data);
end

function mod.after.on_crap_packet(pid, data)
	handle_packet(pid, data, true);
end

return mod;
