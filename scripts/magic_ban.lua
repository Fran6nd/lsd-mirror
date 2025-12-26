-- magic_ban.lua -- Send banned players to the shadow realm
-- TODO: rename to purgatory
local mod = {};
-- TODO: a lot

magic_ban_msg = [[
You've been banned D:
Appeal at https://nsa.gov/
]]

local function send_map_chunk(pid, data)
	send_packet(pid, "\x13"..data);
end

local function send_state_tc(pid)
	send_packet(pid, "\x0f\x00\x7f\x7f\x7f\x00\x00\x00\x00\x00\x00"..string.rep('\0', 20).."\x01\x00");
	send_packet(pid, "\x09\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00");
end

local banned = {};
-- TODO: iterator
-- TODO: allow next_call to automatically determine which func called it, for more reusable funcs?
-- TODO: make unreliable a flag instead of func?
function mod.send_packet(pid, data)
	if (pid >= 0 and pid < MAX_PLAYERS) then
		if (not banned[pid]) then
			return next_call("send_packet", mod.send_packet)(pid, data);
		end
		return 0;
	else
		if (pid == PID_BROADCAST) then
			for i=0,MAX_PLAYERS-1 do
				if (is_connected(i) and not banned[i]) then
					next_call("send_packet", mod.send_packet)(i, data);
				end
			end
			return 0;
		else
			return next_call("send_packet", mod.send_packet)(pid, data);
		end
	end
end

function mod.send_packet_unreliable(pid, data)
	if (pid >= 0 and pid < MAX_PLAYERS) then
		if (not banned[pid]) then
			return next_call("send_packet_unreliable", mod.send_packet_unreliable)(pid, data);
		end
		return 0;
	else
		if (pid == PID_BROADCAST) then
			for i=0,MAX_PLAYERS-1 do
				if (is_connected(i) and not banned[i]) then
					next_call("send_packet_unreliable", mod.send_packet_unreliable)(i, data);
				end
			end
			return 0;
		else
			return next_call("send_packet_unreliable", mod.send_packet_unreliable)(pid, data);
		end
	end
end

function send_to_purgatory(pid)
	-- TODO: set joined to false, after_destroy and boot
	banned[pid] = true;
	local head = send_packet;
	send_packet = server.send_packet;
	send_map_start(pid, 11);
	send_map_chunk(pid, "\x78\xda\x63\xb0\xb3\x01\x00\x00\xbb\x00\x7b");
	-- TODO: predefined colors?
	-- TODO: {{pos={x,y,z}, team=0},}
	send_state_tc(pid, 0, {"", ""}, {{r=0, g=0, b=0}, {r=0, g=0, b=0}}, {r=127, g=127, b=127}, {});
	-- TODO: reorganize args
	--send_player(0, SPECTATOR, 0, 0, 0, {r=0, g=0, b=0}, "");
	for line in string.gmatch(magic_ban_msg, "([^\n]+)") do
		send_chat(pid, line, 2, 0);
	end
	send_packet = head;
end

-- function mod.send_packet(...)
-- 	return next_call("send_packet", mod.send_packet)(...);
-- end

-- TODO: overridable is_connected which makes all functions, even send_packet, pretend the player isn't there?
-- (though that sounds like a terrible idea. . .)
-- TODO: prevent sending packets here
-- TODO: error if loaded with no module? put commands into the module?
function mod.on_any_connect(pid)
	--banned[pid] = true;
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

	send_to_purgatory(pid);
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

return mod;
