-- command_savemap.lua -- Output a VXL file into a predetermined directiory
local mod = init_mod();

getcfg("command_savemap_basedir", "rw/maps/");
getcfg("command_savemap_suffix", ".vxl");

local bad_filename_msg = {
	en="name must not contain '/' or '\\', or start with a '.'"
};

local cmd = {name="savemap", caps="savemap", fakepid=true, usage="name", desc="Write a VXL file with the current map's state."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local name = argv[1];

	if (string.find(name, "[/\\]") or string.find(name, "^%.")) then
		l10n_send_chat(pid, bad_filename_msg);
		return;
	end

	local path = command_savemap_basedir..argv[1]..command_savemap_suffix;
	local tmppath = path..".new";

	local file = io.open(tmppath, "wb");
	file:setvbuf("no");

	for dat in dump_vxl() do
		file:write(dat);
	end

	file:close();
	os.rename(tmppath, path);
end
register_command(cmd, mod);

return mod;
