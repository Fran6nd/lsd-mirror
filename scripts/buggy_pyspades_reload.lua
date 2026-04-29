-- buggy_pyspades_reload.lua -- Let players continue reloading without holding a gun
local mod = init_mod();
local fired;

function mod.after.before_estimated_fire()
	fired = true;
end

function mod.set_tool(pid, tool)
	local rltime = get_reload_time(pid);
	fired = nil;

	mod.next.set_tool(pid, tool);

	if (not fired) then
		set_reload_time(pid, rltime);
	end
end

-- TODO: hammer out naming inconsistencies
function mod.on_tool_change(pid, tool)
	local rltime = get_reload_time(pid);
	fired = nil;

	mod.next.on_tool_change(pid, tool);

	if (not fired) then
		set_reload_time(pid, rltime);
	end
end

return mod;
