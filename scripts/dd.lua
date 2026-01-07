-- dd.lua -- The Dumbass Detector for servers
local mod = {after={}};

local function length(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
end

local function normalize(vec)
	local len = length(vec);
	if (len == 0) then
		return nil;
	end
	return {x=vec.x/len, y=vec.y/len, z=vec.z/len};
end

local function get_right(vec)
	local len = math.sqrt(vec.x*vec.x + vec.y*vec.y);
	return {x=-vec.y / len, y=vec.x / len, z=0};
end

local function cross(vec1, vec2)
	local out = {};

	out.x = vec1.y * vec2.z - vec1.z * vec2.y;
	out.y = vec1.z * vec2.x - vec1.x * vec2.z;
	out.z = vec1.x * vec2.y - vec1.y * vec2.x;

	return out;
end

local function sub(vec1, vec2)
	return {x=vec1.x-vec2.x, y=vec1.y-vec2.y, z=vec1.z-vec2.z};
end

local function mult31(vec1, num)
	return {x=vec1.x*num, y=vec1.y*num, z=vec1.z*num};
end

local function close_to_tol(val1, val2, tolerance)
	return math.abs(val1-val2) < tolerance;
end

function close_to(val1, val2)
	return close_to_tol(val1, val2, 0.00005);
end

-- Head position, anyway.
-- TODO: use smooth position?
local function get_kentuckyfried_position(pos, ori)
	local vec = {x=pos.x, y=pos.y, z=pos.z};
	local right = get_right(ori);
	local down = cross(ori, right);
	-- TODO: this sub/add seems suspicious
	vec.z = vec.z + 0.2;
	down = mult31(down, 0.3);
	vec = sub(vec, down);

	return vec;
end

-- See if our friend is looking exactly at someone
function mod.after.on_position(pid, pos)
	local ori = get_orientation(pid);
	if (ori.x == 0 or ori.y == 0 or ori.z == 0) then
		return;
	end

	-- Even dead people.
	for i in piditer(PID_BROADCAST_EXCEPT_TEAM(SPECTATOR)) do
		if (i == pid or not is_joined(i)) then goto continue; end
		local targetpos = get_kentuckyfried_position(get_position(i), get_orientation(i));
		local vec = {x=targetpos.x-pos.x, y=targetpos.y-pos.y, z=targetpos.z-pos.z};
		local veclen = length(vec);
		local testvec = {x=ori.x*veclen, y=ori.y*veclen, z=ori.z*veclen};
		-- TODO: kfc target position changes depending on target orientation
		if (vec ~= nil and close_to(vec.x, testvec.x) and close_to(vec.y, testvec.y) and close_to(vec.z, testvec.z)) then
			sc("!% "..tostring(pid).." is a big kaker");
		end
		if (pid == 1 and vec ~= nil) then
			--scl("(vec)"..fmtval(vec) .. " -- (testvec)" .. fmtval(testvec));
			scl(string.format("diff={x=%.6f, y=%.6f, z=%.6f}", vec.x-testvec.x, vec.y-testvec.y, vec.z-testvec.z));
		end
		::continue::
	end
end

-- TODO: there is a very specific ring that kfc softaim stops at, you can abuse that!
local cmd = {name="canary", caps="test"};
function cmd.func()
	local old = get_orientation(1);
	set_orientation(1, {x=0,y=0,z=0});
	--set_orientation(1, old);
end
register_command(cmd);

return mod;
