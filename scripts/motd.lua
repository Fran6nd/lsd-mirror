-- motd.lua -- Send a blob of text to players when they join
local mod = {after={}};
getcfg("motd", "Server owner forgot to set the motd, oh no");

-- TODO: first join only -- on_initial_join
function mod.after.on_join(pid)
	for line in string.gmatch(motd, "([^\n]+)") do
		send_chat(pid, line, 2, 0);
	end
end

return mod;
