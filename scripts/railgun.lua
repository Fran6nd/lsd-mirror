-- railgun.lua -- Blast holes in things.
local mod = {after={}};

-- Some swizzle functions
local function sign1(num)
	return num < 0 and -1 or 1;
end

local function sign(vec)
	return {x=sign1(vec.x), y=sign1(vec.y), z=sign1(vec.z)};
end

local function abs(vec)
	return {x=math.abs(vec.x), y=math.abs(vec.y), z=math.abs(vec.z)};
end

local function sub(vec1, vec2)
	return {x=vec1.x-vec2.x, y=vec1.y-vec2.y, z=vec1.z-vec2.z};
end

local function add(vec1, vec2)
	return {x=vec1.x+vec2.x, y=vec1.y+vec2.y, z=vec1.z+vec2.z};
end

local function div(vec1, vec2)
	return {x=vec1.x/vec2.x, y=vec1.y/vec2.y, z=vec1.z/vec2.z};
end

local function div13(num, vec2)
	return {x=num/vec2.x, y=num/vec2.y, z=num/vec2.z};
end

local function floor(vec)
	return {x=math.floor(vec.x), y=math.floor(vec.y), z=math.floor(vec.z)};
end

local function max31(vec1, num)
	return {x=math.max(vec1.x, num), y=math.max(vec1.y, num), z=math.max(vec1.z, num)};
end

local function length(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
end

-- TODO: to core?
local function blocks_to_destroy(pos)
	for z=pos.z-1,pos.z+1 do
		if (z < 0 or z > 63) then goto continuez; end
		for y=pos.y-1,pos.y+1 do
			if (y < 0 or y > 511) then goto continuey; end
			for x=pos.x-1,pos.x+1 do
				if (x >= 0 and x <= 511 and is_solid{x=x, y=y, z=z}) then
					return true;
				end
			end
			::continuey::
		end
		::continuez::
	end
	
	return false;
end

-- TODO: killing people too close to you does not go well
-- 	TODO: replace with send + kill, except_team_and_player
local function kill_people(pid, pos, dist)
	for i in piditer(PID_BROADCAST_EXCEPT_TEAM(get_team(pid))) do
		local ipos = get_position(i);
		if (is_alive(i) and length(sub(pos, ipos)) < dist) then
			detonate_grenade(spawn_grenade(pid, get_team(pid), ipos, {x=0, y=0, z=0}, 0));
		end
	end
end

local function cast(pid, start, off)
	local step;
	local delta;
	local tmax;
	local vox;
	local hasdestroyed = false;
	local killdist;

	-- TODO: do i need maxblocks to limit length?

	step = sign(off);
	delta = abs(div13(1, off));
	tmax = div(add(sub(floor(start), start), max31(sign(off), 0)), off);
	vox = floor(start);
	while (true) do
		if ((step.z == -1 and vox.z < 0) or (step.z == 1 and vox.z > 63) or
		    vox.x < 0 or vox.x > 511 or
		    vox.y < 0 or vox.y > 511) then
			return;
		end

		-- TODO: original had 4 and 8 reversed; why?
		killdist = hasdestroyed and 4 or 8;
		if (blocks_to_destroy(vox)) then
			block_action(vox, 3, 0);
			hasdestroyed = true;
		end
		kill_people(pid, vox, killdist);

		if (tmax.z <= tmax.x and tmax.z <= tmax.y) then
			vox.z = vox.z + step.z;
			tmax.z = tmax.z + delta.z;
		elseif (tmax.x < tmax.y) then
			vox.x = vox.x + step.x;
			tmax.x = tmax.x + delta.x;
		else
			vox.y = vox.y + step.y;
			tmax.y = tmax.y + delta.y;
		end
	end
end

local cmd = {name="bore"};
function cmd.func(pid)
	cast(get_position(pid), get_orientation(pid));
end
register_command(cmd);

-- TODO: better combined before/after?
function mod.on_mouse_input(pid, bitmask)
	local oldinp = get_mouse_inputs(pid);
	next_call("on_mouse_input", mod.on_mouse_input)(pid, bitmask);

	if (get_tool(pid) ~= 3 or bit.band(oldinp, 2) == 2 or bit.band(bitmask, 2) ~= 2) then
		return;
	end

	local pos = get_position(pid);
	send_grenade(PID_BROADCAST, pos, {x=0,y=0,z=0}, 0, pid);
	cast(pid, pos, get_orientation(pid));
end

return mod;
