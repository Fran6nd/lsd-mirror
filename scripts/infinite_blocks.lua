-- infinite_blocks.lua -- Blocks (and grenades) never run out
local mod = {after={}};
-- TODO: should i fill struct Player with 0/bogus values on disconnect?

local function refill(pid)
	-- TODO: set tool to block if none left
	local oldhp = get_hp(pid);
	restock(pid);
	set_hp(pid, oldhp);
end

--void (*set_ammo)(plid pid, unsigned mag, unsigned reserve, struct State *st);
-- TODO: pretty sure XOR:
-- 1. babel should go on top of this
-- 2. babel should throw an error or something to exit the chain
function mod.after.on_block_action(pid, pos, type)
	if (type == 0) then
		refill(pid);
	end
end

function mod.after.on_block_line(pid)
	refill(pid);
end

return mod;
