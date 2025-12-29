-- infinite_blocks.lua -- Blocks (and grenades) never run out
local mod = {after={}};
-- TODO: should i fill struct Player with 0/bogus values on disconnect?

-- TODO: pretty sure XOR:
-- 1. babel should go on top of this
-- 2. babel should throw an error or something to exit the chain
function mod.after.on_block_action(pid, pos, type)
	if (type == 0) then
		restock(pid);
	end
end

return mod;
