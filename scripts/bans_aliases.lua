-- bans_aliases.lua -- hban, dban, wban, mban, yban, pban and more!
local mod = init_mod();

local function tmban(pid, cmd, argv, duration)
	cmd_assert(pid, cmd, #argv >= 2);

	local banpid = get_arg_pid("player", pid, cmd, argv[1]);
	local comment = table.concat(argv, " ", 2);

	bans_ban_player(pid, banpid, duration, comment, bans_default_caps);
end

local function tmbanip(pid, cmd, argv, duration)
	cmd_assert(pid, cmd, #argv >= 2);

	local starta, enda = get_arg_cidr("range", pid, cmd, argv[1]);
	local comment = table.concat(argv, " ", 2);

	bans_ban_addr(pid, starta, enda, nil, duration, comment, bans_default_caps);
end

local cmd = {name="hban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for an hour."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 60*60);
end
register_command(cmd, mod);

local cmd = {name="dban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for a day."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 60*60*24);
end
register_command(cmd, mod);

local cmd = {name="wban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for 7 days."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 60*60*24*7);
end
register_command(cmd, mod);

local cmd = {name="mban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for 30 days."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 60*60*24*30);
end
register_command(cmd, mod);

local cmd = {name="yban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for 365 days."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 60*60*24*365);
end
register_command(cmd, mod);

local cmd = {name="pban", caps="bans", fakepid=true, usage="player comment...", desc="Ban a naughty player for a really long time."};
function cmd.func(pid, argv)
	tmban(pid, cmd, argv, 2^50);
end
register_command(cmd, mod);

local cmd = {name="hbanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for an hour."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 60*60);
end
register_command(cmd, mod);

local cmd = {name="dbanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for a day."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 60*60*24);
end
register_command(cmd, mod);

local cmd = {name="wbanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for 7 days."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 60*60*24*7);
end
register_command(cmd, mod);

local cmd = {name="mbanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for 30 days."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 60*60*24*30);
end
register_command(cmd, mod);

local cmd = {name="ybanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for 365 days."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 60*60*24*365);
end
register_command(cmd, mod);

local cmd = {name="pbanip", caps="bans", fakepid=true, usage="range comment...", desc="Ban a naughty IPv4 address or CIDR range for a really long time."};
function cmd.func(pid, argv)
	tmbanip(pid, cmd, argv, 2^50);
end
register_command(cmd, mod);

return mod;
