-- motd.lua -- Send a blob of text to players when they join
local mod = {after={}};
getcfg("motd", "Server owner forgot to set the motd, oh no");

local function send_motd(pid)
	for line in string.gmatch(motd, "([^\n]+)") do
		server_msg(pid, line);
	end
end

-- TODO: first join only -- on_initial_join
function mod.after.on_join(pid)
	send_motd(pid);
end

function mod.after.on_fakepid_connect(pid)
	send_motd(pid);
end

return mod;
