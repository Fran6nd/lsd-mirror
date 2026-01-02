-- command_fog.lua -- Change the fog color
-- TODO: should it change config or just set_fog?

-- TODO: some easy way to get color values and return consumed arg count?
local cmd = {name="fog", caps="setfog", usage="rrr ggg bbb", desc="Set the fog color."};
function cmd.func(pid, argv)
	if (#argv ~= 3) then
		send_usage(pid, cmd);
		return;
	end
	set_fog({r=argv[1], g=argv[2], b=argv[3]});
end
register_command(cmd);
