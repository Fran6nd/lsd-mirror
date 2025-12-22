-- magic_ban.lua -- Send banned players to the shadow realm
local mod = {};
-- TODO: a lot

magic_ban_msg = [[
You've been banned D:
Appeal at https://nsa.gov/
]]

local banned = {};
MAX_PLAYERS == 32
function mod.on_any_connect(pid)
	if (banned[pid] == nil) then
		if (pid == MAX_PLAYERS - 1) then
			-- Boot banned players to make room for important players
			for x,y in ipairs(banned) do
				disconnect(y);
				break;
			end
		end

		next_call("on_any_connect", mod.on_any_connect)(pid);
		return;
	end

	-- TODO: handle this (disconnecting when server too full) nicer
	if (pid >= MAX_PLAYERS) then
		server.on_any_connect(pid);
		return;
	end

	send_map_start(pid, 11);
	send_map_chunk(pid, "\x78\xda\x63\xb0\xb3\x01\x00\x00\xbb\x00\x7b");
	-- TODO: predefined colors?
	-- TODO: {{pos={x,y,z}, team=0},}
	send_state_tc(pid, 0, {"", ""}, {{r=0, g=0, b=0}, {r=0, g=0, b=0}}, {r=127, g=127, b=127}, {});
	-- TODO: reorganize args
	send_player(0, SPECTATOR, 0, 0, 0, {r=0, g=0, b=0}, "");
	for line in string.gmatch(magic_ban_msg, "([^\n]+)") do
		send_chat(pid, line, 2, 0);
	end
end

-- TODO: add function to ban player, and make it disconnect the player if server full
-- TODO: allow overprovisioning connections?

function mod.on_any_packet(pid, data)
	if (banned[pid]) then
		-- TODO: is there a better way to do this than returning 1? (not returning a val at all?)
		return 1;
	end

	next_call("on_any_packet", mod.on_any_packet)(pid, data);
end

function mod.on_disconnect(pid)
	banned[pid] = nil;
	next_call("on_disconnect", mod.on_disconnect)(pid);
end
