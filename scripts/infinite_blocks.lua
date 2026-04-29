-- infinite_blocks.lua -- Blocks (and grenades) never run out
local mod = init_mod();
-- TODO: should i fill struct Player with 0/bogus values on disconnect?

local function refill(pid)
	-- TODO: set tool to block if none left
	local oldhp = get_hp(pid);
	local oldreserve = get_reserve_ammo(pid);
	-- Mag ammo isn't affected by restock()

	restock(pid);
	set_hp(pid, oldhp);
	set_ammo(pid, get_mag_ammo(pid), oldreserve);
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

function mod.after.on_block_line(pid, startp, endp)
	refill(pid);
	if (1 + math.abs(endp.x - startp.x) + math.abs(endp.y - startp.y) + math.abs(endp.z - startp.z) >= 50) then
		-- TODO: did this thing work on voxlap? better go fix it if not
		set_tool(pid, 1);
	end
end

return mod;
