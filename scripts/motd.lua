-- motd.lua -- Send a blob of text to players when they join
local mod = {};

-- TODO: first join only -- on_initial_join
function mod.on_join(pid, team, weapon, name)
	next_call("on_join", mod.on_join)(pid, team, weapon, name);

	for line in string.gmatch(motd, "([^\n]+)") do
		send_chat(pid, line, 2, 0);
	end
end

return mod;
