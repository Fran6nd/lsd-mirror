-- noclip.lua -- when i'm feeling tired i can just... fly around
local mod = {after={}};

local clips = {};
-- TODO: should jumpctr be player-specific or global?
local jumpctr = {};
function mod.after.on_join(pid)
	clips[pid] = nil;
	jumpctr[pid] = 0;
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

-- TODO: this doesn't account for velocity when the player is dropped out of noclip
-- TODO: should i really be using bit.band for all of this?
-- TODO: vanilla openspades has funky sprint behavior, should i look at that?
-- TODO: allow configurable speed (maybe even in the command?)
-- TODO: should i prevent SBAD from sending position set packets?
-- TODO: should i try to reset the player's velocity or increase tickrate?
-- TODO: 	you can use set_jump for that
-- TODO: will this look trash with <60 Hz wupd?
-- TODO: if you're stuck in a block you're forced into crouching
-- TODO: should i (optionally?) flatten the ori and down vectors?
local function noclip_phys(pid, delta)
	local pos = get_position(pid);
	local ori = get_orientation(pid);
	local inputs = get_inputs(pid);
	local right = get_right(ori);
	local down = cross(ori, right);

	local len = delta * 16;
	if (bit.band(inputs, 128) == 128) then
		len = len * 2;
	end

	-- If jumping in air was possible I'd make the sneak key slow things down.
	-- Unfortunately it's not possible so the sneak key is the new jump key.
	-- if (bit.band(inputs, 64) == 64) then
	-- 	len = len / 2;
	-- end

	-- TODO: /sqrt(2)?
	local forwardlen = len * (bit.band(inputs, 1) + bit.band(inputs, 2)/-2);
	local rightlen = len * (bit.band(inputs, 8)/8 + bit.band(inputs, 4)/-4);
	local downlen = len * (bit.band(inputs, 32)/32 + bit.band(inputs, 64)/-64);

	ori = {x=ori.x*forwardlen, y=ori.y*forwardlen, z=ori.z*forwardlen};
	right = {x=right.x*rightlen, y=right.y*rightlen, z=right.z*rightlen};
	down = {x=down.x*downlen, y=down.y*downlen, z=down.z*downlen};

	new = {x=pos.x + ori.x + right.x + down.x, y=pos.y + ori.y + right.y + down.y, z=pos.z + ori.z + right.z + down.z};

	-- Loop through map borders. TODO: optional?
	-- is there even a need with the crap packet destroyer (not going to look for haxors in the border)?
	new = {x=new.x % 512, y=new.y % 512; z=new.z};

	set_position(pid, new);

	-- Try to make the view less jittery.
	jumpctr[pid] = jumpctr[pid] + delta;
	if (jumpctr[pid] >= 0.5) then
		set_jump(pid);
		jumpctr[pid] = jumpctr[pid] - 0.5;
	end
end

-- TODO: on_crap_packet silencing for scripts?
function mod.on_position(pid, delta)
	if (not clips[pid]) then
		next_call("on_position", mod.on_position)(pid, delta);
	end
end

-- TODO: pretty sure jump is forced on until /noclip is disabled
function mod.tick_player_physics(pid, delta)
	if (clips[pid]) then
		noclip_phys(pid, delta);
		return;
	end

	next_call("tick_player_physics", mod.tick_player_physics)(pid, delta);
end

local cmd = {name="noclip", caps="noclip"};
function cmd.func(pid)
	clips[pid] = not clips[pid];
	jumpctr[pid] = 0;
end
register_command(cmd);

return mod;
