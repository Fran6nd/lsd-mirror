-- jp.lua -- An implementation of that jetpack command
local mod = {after={},before={}};

local flies = {};
function mod.after.on_join(pid)
	flies[pid] = nil;
end

function mod.before.tick()
	for i in piditer(PID_BROADCAST) do
		if (flies[i] and bit.band(get_inputs(i), 64) == 64) then
			set_jump(i);
		end
	end
end

local cmd = {name="jp", caps="jp"};
function cmd.func(pid)
	flies[pid] = not flies[pid];
end
register_command(cmd);

return mod;
