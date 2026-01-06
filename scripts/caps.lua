-- caps.lua -- Capabilities, or random flags you can assign to players that let them do/not do things
-- TODO: allow adding values to caps? i.e. login=<username>, ban=1h, etc.
-- TODO: handle the last TODO nicely with "all", probably the value will be a specified limitation if not true -- all can do whatever
-- TODO: about the last 2 TODO, maybe use commas to separate things? or just make whatever after the = unspecified but not whitespace
-- TODO: special: prefix, so root doesn't get it? but how would that work with i.e. login? make a user and login cap? would it be easier to use a real table?
-- TODO: you probably DO want to keep the =, for things like limiting ban time (TODO: how would you validate that the cap is formatted correcty?), but should defer user to auth.lua
-- TODO: auth uses caps="login" a lot, how should that be handled?
local mod = {after={}};
getcfg("cap_groups", {
	guard = {
		"cmd:advance",
		"ban=1h",
		"some_limited_ban_cap"
	},
	-- Mods can do whatever guards do and more
	mod = {
		"guard",
		"jp",
		"noclip"
	},
	-- Admin's just an alias for "all"
	admin = {
		"all"
	},
	-- The nerd group is the most powerful group here
	nerd = {
		"exec",
		"modutils"
	},
	-- A player with this group can take a stroll and not much more.
	["badcap:neuter"] = {
		"badcap:mute",
		"badcap:nobuild",
		"badcap:nodamage"
	}
});

-- caps is recursively calculated from groups
local groups = {};
local caps = {};

-- TODO: wonder how "all" would be handled here; should the cap be kept a secret and users be forced to check every grant/drop?
function on_cap_grant(pid, cap)end
server.on_cap_grant = on_cap_grant;

-- TODO: cap drop is even more of a mystery with subcaps
function on_cap_drop(pid, cap)end
server.on_cap_drop = on_cap_drop;

local function parse_cap_val(str)
	local cap, val = string.match(str, "(.-)=?(.*)");
	if (cap == "") then
		cap = val;
		val = true;
	end
	return cap, val;
end

-- TODO: recursion refuses to work
local function grant_subcaps(pid, cap)
	local cap, val = parse_cap_val(cap);

	if (caps[pid][cap] == nil) then
		caps[pid][cap] = val;
		if (cap_groups[cap]) then
			for x in ipairs(cap_groups[cap]) do
				-- Try not to make infinite loops.
				grant_subcaps(pid, x);
			end
		end
		on_cap_grant(pid, cap);
	end
end

local function grant_cap_val(pid, cap, val)
	if (groups[pid] == nil) then
		groups[pid] = {};
		caps[pid] = {};
	end

	groups[pid][cap] = val;
	grant_subcaps(pid, cap);
end

function grant_cap(pid, cap)
	grant_cap_val(pid, parse_cap_val(cap));
end

function drop_cap(pid, cap)
	if (groups[pid][cap]) then
		groups[pid][cap] = nil;
		caps[pid] = {};
		for x,_ in pairs(groups[pid]) do
			grant_subcaps(pid, x);
		end
		on_cap_drop(pid, cap);
	end
end

function toggle_cap(pid, cap)
	if (has_cap(pid, cap)) then
		drop_cap(pid, cap);
		return false;
	end
	grant_cap(pid, cap);
	return true;
end

function has_cap(pid, cap)
	return caps[pid] ~= nil and (caps[pid][cap] or (caps[pid].all and string.sub(cap, 1, 7) ~= "badcap:"));
end

function get_cap_groups(pid)
	if (groups[pid] == nil) then
		return "";
	end

	local str = "";
	local sep = "";
	for x,_ in pairs(groups[pid]) do
		str = str .. sep .. x;
		sep = " ";
	end

	return str;
end

function get_caps(pid)
	if (caps[pid] == nil) then
		return "";
	end

	local str = "";
	local sep = "";
	for x,_ in pairs(caps[pid]) do
		str = str .. sep .. x;
		sep = " ";
	end

	return str;
end

function mod.on_load()
	groups = {};
	caps = {};
end

-- TODO: need deinit_connection func or something
function mod.after.on_disconnect(pid)
	groups[pid] = {};
	caps[pid] = {};
end

function mod.after.disconnect_now(pid)
	groups[pid] = {};
	caps[pid] = {};
end

local function get_cmd_canonical_name(cmd)
	if (type(cmd.name) == "table") then
		return cmd.name[1];
	end

	return cmd.name;
end

local need_cap_msg = {
	en="You need the %(cap) capability to run that command."
};

-- Abusing register to interface cleanly with commands.lua
function mod.try_run_command(cmd, pid, argv, msg)
	if (cmd.caps ~= nil) then
		if (not has_cap(pid, cmd.caps) and not has_cap(pid, "cmd:"..get_cmd_canonical_name(cmd))) then
			l10n_send_chat(pid, need_cap_msg);
			return;
		end
	end

	next_call("try_run_command", mod.try_run_command)(cmd, pid, argv, msg);
end

function mod.can_see_command(pid, cmd)
	if (not next_call("can_see_command", mod.can_see_command)(pid, cmd)) then
		return false;
	end

	return cmd.caps == nil or has_cap(pid, cmd.caps) or has_cap(pid, "cmd:"..get_cmd_canonical_name(cmd));
end

return mod;
