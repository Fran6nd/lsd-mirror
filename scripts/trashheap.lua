-- trashheap.lua -- trash not a burner likes to /exec

function sc(x)send_chat(PID_BROADCAST,tostring(x),2,0)end

ticker = {};
local before;
function ticker.on_load()
	before = get_time();
end

function ticker.tick()
	local now = get_time();

	next_call("tick", ticker.tick)();
	
	if true then
		--log("time: %f", now);
		log("timediff:      %f\ntimediff-1/60: %f\n", now - before, now - before - 0.01666666666666666666);
	elseif (now - before > 0.019) then
		-- That extra \n at the end is intentional.
		log("timediff:      %f\ntimediff-1/60: %f\n", now - before, now - before - 0.01666666666666666666);
	end

	before = now;
end

whereami = {};
function whereami.tick()
	next_call("tick", whereami.tick)();
	
	local pos = get_position(0);
	send_chat(0, string.format("N%% pos: {%.3f, %.3f, %.3f}", pos.x, pos.y, pos.z), 2, 0);
end

return {};
