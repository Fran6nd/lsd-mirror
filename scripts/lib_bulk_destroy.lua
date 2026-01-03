-- bulk_destroy.lua -- Pretty API to destroy lots of blocks at once without slowing to a crawl
-- TODO: -> lua.c

local posarr = {};
local maskarr = {};
function bdestroy_block_action(pos, type)
	local mask = block_action_rm(pos, type, 0);
	if (mask ~= 0) then
		table.insert(posarr, {x=pos.x, y=pos.y, z=pos.z});
		table.insert(maskarr, mask);
		return true;
	end
	return false;
end

function bdestroy_finish()
	for i=1,#posarr do
		block_action_cull(posarr[i], maskarr[i]);
	end
	posarr = {};
	maskarr = {};
	finish_cull();
end
