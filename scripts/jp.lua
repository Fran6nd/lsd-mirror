-- jp.lua -- An implementation of that jetpack command
local mod = {};

local flies = {};
function mod.on_join(pid, team, weapon, name)
	next_call("on_join", mod.on_join)(pid, team, weapon, name);
	flies[pid] = nil;
end

MAX_PLAYERS=32
function mod.tick()
	for i=0,MAX_PLAYERS-1 do
		if (flies[i] and bit.band(get_inputs(i), 64) == 64) then
			set_jump(i);
		end
	end

	next_call("tick", mod.tick)();
end

local cmd = {name="jp"};
function cmd.func(pid)
	flies[pid] = not flies[pid];
end
register_command(cmd);

return mod;
