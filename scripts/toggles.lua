-- toggles.lua -- Toggle building, killing, talking, moving, etc.
local mod = init_mod();
-- TODO: automute based on regexp, muted player only knows after he's sent a few messages but normal players never see anything

local added_msg = {
	en="Added %(cap) to %(name)"
};

local removed_msg = {
	en="Removed %(cap) from %(name)"
};

-- TODO: infinite_blocks with voxlap?
function mod.early.on_block_action(pid, pos, type)
	if (has_cap(pid, "badcap:nobuild")) then
		return;
	end
	mod.early.next.on_block_action(pid, pos, type);
end

function mod.early.on_block_line(pid, pstart, pend)
	if (has_cap(pid, "badcap:nobuild")) then
		return;
	end
	mod.early.next.on_block_line(pid, pstart, pend);
end

-- TODO: use ... for args i don't care about? (and extension args?)
function mod.early.on_hit(pid, type, hitPlayer)
	if (has_cap(pid, "badcap:nodamage")) then
		return;
	end
	mod.early.next.on_hit(pid, type, hitPlayer);
end

-- TODO: need a level after early?
function mod.send_chat(pid, msg, type, from)
	if (has_cap(from, "badcap:mute")) then
		return;
	end
	mod.next.send_chat(pid, msg, type, from);
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
local cmd = {name={"togglebuild", "tb"}, caps="toggles", fakepid=true, usage="player", desc="Prevent a player from directly altering the map."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:nobuild");
end
register_command(cmd);

-- TODO: grenade damage
-- TODO: wonder how that would work with apoc
local cmd = {name={"togglekill", "tk"}, caps="toggles", fakepid=true, usage="player", desc="Prevent a player from directly damaging others."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:nodamage");
end
register_command(cmd);

-- TODO: muted player should probably be able to run commands
local cmd = {name={"mute", "togglechat", "tc"}, caps="toggles", fakepid=true, usage="player", desc="Shut a noisy player up."};
function cmd.func(pid, argv)
	toggle(pid, cmd, argv, "badcap:mute");
end
register_command(cmd);

return mod;
