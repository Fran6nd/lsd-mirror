-- command_version.lua -- Dump the server's name and commit hash
local mod = init_mod();

local version_msg = {
	en="Running %(server_name) commit %(commit)"
}

local cmd = {name="version", fakepid=true, desc="Print the server's version."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	l10n_send_chat(pid, version_msg, {server_name="LSd", commit=GIT_COMMIT});
end
register_command(cmd, mod);

return mod;

