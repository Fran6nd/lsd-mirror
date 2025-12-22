-- jp.lua -- An implementation of that jetpack command
local mod = {};

-- TODO: on_connect, on_disconnect, maybe make a cleaner method of clearing things
local flies = {};

local cmd = {name="jp"};
function cmd.func(pid)
	flies[pid] = not flies[pid];
end
register_command(cmd);

MAX_PLAYERS=32
function mod.tick()
	for i=0,MAX_PLAYERS-1 do
		if (flies[i] and bit.band(get_inputs(i), 64) == 64) then
			set_jump(i);
		end
	end

	next_call("tick", mod.tick)();
end

return mod;
