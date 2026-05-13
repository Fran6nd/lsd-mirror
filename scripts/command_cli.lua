-- command_cli.lua -- Fingerprint random people!

local no_cli_msg = {
	en="%(name): not reported"
}

local cli_handshake_only_msg = {
	en="%(name): handshaked"
}

local cli_msg = {
	en="%(name): '%(char)' (%(client)) v%(major).%(minor).%(patch): %(msg)"
}

local cli_nohandshake_msg = {
	en="%(name): (no handshake) '%(char)' (%(client)) v%(major).%(minor).%(patch): %(msg)"
}

local cli_fakepid_msg = {
	en="%(name): fakepid"
}

-- TODO: nuke this and add a real api
local climap = {
	['B']="BetterSpades",
	['D']="aos.dll",
	['K']="KyroSpades",
	['a']="ACE",
	['o']="OpenSpades"
};

local cmd = {name={"client", "cli", "clin", "client_info"}, fakepid=true, usage="[player]", desc="Print whatever a player's client claims to be."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);
	local player = get_arg_pid_opt("player", pid, cmd, argv[1]) or pid;

	if (is_fakepid(player)) then
		-- Chances are player can only be a fakepid if player refers to pid and argv[1] is nil
		l10n_send_chat(pid, cli_fakepid_msg, {name=get_name(player)});
		return;
	end

	local char = get_client_char(player);
	if (char ~= nil) then
		-- Try to sanitize the char
		if (char < 0x20 or char >= 0x7f) then
			char = string.format("\\x%02x", char);
		elseif (char == "'") then
			char = "\\'";
		else
			char = string.char(char);
		end

		local msg = cli_nohandshake_msg;
		if (get_client_handshaked(player)) then
			msg = cli_msg;
		end

		l10n_send_chat(pid, msg, {name=get_name(player), char=char, client=climap[char] or "?", major=get_client_major(player), minor=get_client_minor(player), patch=get_client_patch(player), msg=get_client_msg(player)});
	else
		if (get_client_handshaked(player)) then
			l10n_send_chat(pid, cli_handshake_only_msg, {name=get_name(player)});
		else
			l10n_send_chat(pid, no_cli_msg, {name=get_name(player)});
		end
	end
end
register_command(cmd);

return {};
