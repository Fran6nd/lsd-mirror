-- command_ip.lua -- Print a player's IPv4 address.
local mod = init_mod();
local bit = require("bit");

local addr_msg = {
	en="%(name): %(addr)"
}

-- TODO: deduplicate with bans.lua
local function fmt_addr(addr)
	return string.format(
		"%u.%u.%u.%u",
		bit.rshift(addr, 24),
		bit.band(bit.rshift(addr, 16), 255),
		bit.band(bit.rshift(addr, 8), 255),
		bit.band(addr, 255)
	);
end


local cmd = {name="ip", caps="ip", fakepid=true, usage="player", desc="Print a player's IP address."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local who = get_arg_pid("player", pid, cmd, argv[1]);

	l10n_send_chat(pid, addr_msg, {
		name=get_name(who),
		addr=fmt_addr(get_ipaddr(who))
	});
end
register_command(cmd, mod);

return mod;
