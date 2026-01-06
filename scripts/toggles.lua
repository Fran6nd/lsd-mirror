-- toggles.lua -- Toggle building, killing, talking, moving, etc.
require "lib_l10n";
local mod = {};
-- TODO: automute based on regexp, muted player only knows after he's sent a few messages but normal players never see anything

local added_msg = {
	en="Added %(cap) to %(name)"
};

local removed_msg = {
	en="Removed %(cap) from %(name)"
};

-- TODO: block line, also in babel
function mod.on_block_action(pid, pos, type)
	if (has_cap(pid, "badcap:nobuild")) then
		-- TODO: deal with stop_exec's funkiness
		--stop_exec();
		return;
	end
	next_call("on_block_action", mod.on_block_action)(pid, pos, type);
end

-- TODO: use ... for args i don't care about? (and extension args?)
function mod.on_hit(pid, type, hitPlayer)
	if (has_cap(pid, "badcap:nodamage")) then
		return;
	end
	next_call("on_hit", mod.on_hit)(pid, type, hitPlayer);
end

-- TODO: make this loaded later than commands
function mod.on_chat(pid, msg, type)
	if (has_cap(pid, "badcap:mute")) then
		return;
	end
	next_call("on_chat", mod.on_chat)(pid, msg, type);
end

local function toggle(pid, cmd, argv, cap)
	local who = get_arg_pid("player", pid, cmd, argv[1]);
	local nowactive = toggle_cap(who, cap);
	l10n_send_chat(pid, nowactive and added_msg or removed_msg, {cap=cap, name=get_name(who)});
end

-- TODO: get pike-style player instead of toggling me
-- TODO: disable family of commands
-- TODO: can i toggle other people with the toggles cap?
-- TODO: hook toggles into bans
local cmd = {name={"togglebuild", "tb"}, caps="toggles", usage="player", desc="Prevent a player from directly altering the map."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:nobuild");
end
register_command(cmd);

-- TODO: grenade damage
-- TODO: wonder how that would work with apoc
local cmd = {name={"togglekill", "tk"}, caps="toggles", usage="player", desc="Prevent a player from directly damaging others."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:nodamage");
end
register_command(cmd);

-- TODO: muted player should probably be able to run commands
local cmd = {name={"mute", "togglechat", "tc"}, caps="toggles", usage="player", desc="Shut a noisy player up."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:mute");
end
register_command(cmd);

return mod;
