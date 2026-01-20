-- command_fog.lua -- Change the fog color
-- TODO: should it change config or just set_fog?
require "lib_l10n";

-- TODO: you need to login to set the fog color?
local need_cap_msg = {
	en="You need the %(cap) capability to set the fog color."
};

-- Displays like "The current fog color is {r=32, g=64, b=128} (#204080)."
local color_msg = {
	en="The current fog color is {r=%(r), g=%(g), b=%(b)} (%(hex))."
};

-- TODO: some easy way to get color values and return consumed arg count?
local cmd = {name="fog", fakepid=true, usage="r g b", desc="Set the fog color."};
function cmd.func(pid, argv)
	if (argv[1]) then
		if (not has_cap(pid, "fog")) then
			l10n_send_chat(pid, need_cap_msg, {cap="fog"});
			return;
		end

		cmd_assert(pid, cmd, #argv == 3);
		set_fog{
			r=get_arg_num_range("r", pid, cmd, argv[1], 0, 255),
			g=get_arg_num_range("g", pid, cmd, argv[2], 0, 255),
			b=get_arg_num_range("b", pid, cmd, argv[3], 0, 255),
		};
		return;
	end

	cmd_assert(pid, cmd, #argv == 0);
	local tbl = get_fog();
	tbl.hex = string.format("#%02x%02x%02x", tbl.r, tbl.g, tbl.b);
	l10n_send_chat(pid, color_msg, tbl);
end
register_command(cmd);

return {};
