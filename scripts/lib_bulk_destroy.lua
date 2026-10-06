-- bulk_destroy.lua -- Pretty API to destroy lots of blocks at once without slowing to a crawl
local mod = init_mod();

local posarr = {};
local maskarr = {};

function mod.impl.bdestroy_block_action(pos, type)
	local mask = block_action_rm(pos, type, get_anon_pid());

	if (mask ~= 0) then
		table.insert(posarr, {x=pos.x, y=pos.y, z=pos.z});
		table.insert(maskarr, mask);

		return true;
	end

	return false;
end

function mod.impl.bdestroy_finish()
	for i=1,#posarr do
		block_action_cull(posarr[i], maskarr[i]);
	end

	posarr = {};
	maskarr = {};

	finish_cull();
end

function mod.impl.bdestroy_session(func, ...)
	local status, err = pcall(func, ...);

	bdestroy_finish();

	if (not status) then
		error(err, 2);
	end
end

return mod;
