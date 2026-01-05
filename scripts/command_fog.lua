-- command_fog.lua -- Change the fog color
-- TODO: should it change config or just set_fog?

-- TODO: some easy way to get color values and return consumed arg count?
local cmd = {name="fog", caps="setfog", usage="r g b", desc="Set the fog color."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 3);
	set_fog{
		r=get_arg_num_range("r", pid, cmd, argv[1], 0, 255),
		g=get_arg_num_range("g", pid, cmd, argv[2], 0, 255),
		b=get_arg_num_range("b", pid, cmd, argv[3], 0, 255),
	};
end
register_command(cmd);
