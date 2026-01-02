-- toggles.lua -- Toggle building, killing, talking, moving, etc.
local mod = {after={}};
local nobuild = {};
local nodamage = {};
local mute = {};
-- TODO: automute based on regexp, muted player only knows after he's sent a few messages but normal players never see anything

function mod.on_load(pid)
	nobuild = {};
	nodamage = {};
	mute = {};
end

function mod.after.on_successful_connect(pid)
	nobuild[pid] = nil;
	nodamage[pid] = nil;
	mute[pid] = nil;
end

function mod.on_block_action(pid, pos, type)
	if (nobuild[pid]) then
		-- TODO: deal with stop_exec's funkiness
		--stop_exec();
		return;
		-- TODO: deal with stop_exec's segfaults
	end
	next_call("on_block_action", mod.on_block_action)(pid, pos, type);
end

-- TODO: use ... for args i don't care about? (and extension args?)
function mod.on_hit(pid, type, hitPlayer)
	if (nodamage[pid]) then
		return;
	end
	next_call("on_hit", mod.on_hit)(pid, type, hitPlayer);
end

-- TODO: make this loaded later than commands
function mod.on_chat(pid, msg, type)
	if (mute[pid]) then
		return;
	end
	next_call("on_chat", mod.on_chat)(pid, msg, type);
end

-- TODO: get pike-style player instead of toggling me
-- TODO: disable family of commands
-- TODO: can i toggle other people with the toggles cap?
-- TODO: hook toggles into bans
local cmd = {name={"togglebuild", "tb"}, caps="toggles"};
function cmd.func(pid, argv)
	nobuild[pid] = not nobuild[pid];
end
register_command(cmd);

local cmd = {name={"togglekill", "tk"}, caps="toggles"};
function cmd.func(pid, argv)
	nodamage[pid] = not nodamage[pid];
end
register_command(cmd);

local cmd = {name={"mute", "togglechat", "tc"}, caps="toggles"};
function cmd.func(pid, argv)
	mute[pid] = not mute[pid];
end
register_command(cmd);

return mod;
