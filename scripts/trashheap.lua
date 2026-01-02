-- trashheap.lua -- trash not a burner likes to /exec

function sc(x)send_chat(PID_BROADCAST,tostring(x),2,0)end
function scl(x)for y in string.gmatch(x,"[^\n]+")do sc(y)end;end
function ptab(tbl, depth)
	if (depth == nil) then
		depth = 0;
	end

	local tab = string.rep("        ", depth);
	local out = "{\n";
	for x,y in pairs(tbl) do
		out = out..tab.."        "..fmtval(x, depth).." = "..fmtval(y, depth)..",\n";
	end
	return out..tab.."}";
end

function fmtval(x, depth)
	if (depth == nil) then
		depth = -1;
	end

	if (type(x) == "string") then
		return '"'..x..'"';
	end

	if (type(x) ~= "table") then
		return tostring(x);
	end

	return ptab(x, depth+1);
end

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

ticker2 = {};
local before2;
function ticker2.on_load()
	before2 = get_time();
end

function ticker2.tick()
	local now = get_time();

	next_call("tick", ticker2.tick)();
	
	if false then
		send_chat(PID_BROADCAST, string.format("N%% %f", now - before2 - 0.01666666666666666666), 2, 0);
	elseif (now - before2 > 0.019) then
		send_chat(PID_BROADCAST, string.format("N%% %f", now - before2 - 0.01666666666666666666), 2, 0);
	end

	before2 = now;
end

whereami = {};
function whereami.tick()
	next_call("tick", whereami.tick)();
	
	local pos = get_position(0);
	send_chat(0, string.format("N%% pos: {%.3f, %.3f, %.3f}", pos.x, pos.y, pos.z), 2, 0);
end

return {};
