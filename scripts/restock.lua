-- restock.lua -- Refill ammo and things at a tent
local mod = {};

-- TODO: near_tent call or something like that
-- TODO: plumb tent positions without piggybacking off of babel.lua
local function within_cylinder(pos, cylinderpos, radius, bottom, top)
	pos.x = pos.x - cylinderpos.x;
	pos.y = pos.y - cylinderpos.y;
	pos.z = pos.z - cylinderpos.z;

	if (length2(pos) > radius or pos.z < top or pos.z > bottom) then
		return false;
	end

	return true;
end

function mod.tick()
	next_call("tick", mod.tick)();

	for i=0,MAX_PLAYERS-1 do
		if (is_alive(i) and within_cylinder(get_position(i), get_tent_position(get_team(i)), 3, 1, -4)) then
			restock(i);
			-- TODO: put some delay on restock
			-- TODO: only bother if there's something *to* restock
			-- TODO: allow aloha.pk babel-style ammo restocking (or maybe just add that as a hook to restock -- TODO: do that without sending 2 ammo packets in a row)
			-- TODO: probably just calculate diffs after every root event. . .
			-- TODO: and before any func which depends on those diffs
			-- TODO: and maybe before any lua send_ execution
			-- TODO: and maybe add a _now variant of non-send funcs
		end
	end
end

return mod;
