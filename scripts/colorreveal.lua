-- colorreveal.lua -- Reveal hidden (dirt) colors to clients
local mod = init_mod();

local function is_hidden(pos)
	neighbors = {
		{x=pos.x-1, y=pos.y  , z=pos.z  },
		{x=pos.x+1, y=pos.y  , z=pos.z  },
		{x=pos.x  , y=pos.y-1, z=pos.z  },
		{x=pos.x  , y=pos.y+1, z=pos.z  },
		{x=pos.x  , y=pos.y  , z=pos.z-1},
		{x=pos.x  , y=pos.y  , z=pos.z+1}
	};

	-- TODO: pos.z == 0 being visible had better be right
	if (not (pos.x >= 0 and pos.x < 512 and pos.y >= 0 and pos.y < 512 and pos.z >= 1 and pos.z < 64)) then
		return false;
	end

	-- TODO: this is messy, hide it somewhere
	for _,p in ipairs(neighbors) do
		if (p.x >= 0 and p.x < 512 and p.y >= 0 and p.y < 512 and p.z >= 0 and p.z < 64 and not is_solid(p)) then
			return false;
		end
	end

	return true;
end

function mod.block_action_rm(pos, type, from)
	local neighbors = {};
	local ret;

	if (type == 1) then
		neighbors = {
			{x=pos.x-1, y=pos.y  , z=pos.z  },
			{x=pos.x+1, y=pos.y  , z=pos.z  },
			{x=pos.x  , y=pos.y-1, z=pos.z  },
			{x=pos.x  , y=pos.y+1, z=pos.z  },
			{x=pos.x  , y=pos.y  , z=pos.z-1},
			{x=pos.x  , y=pos.y  , z=pos.z+1}
		};
	elseif (type == 2) then
		neighbors = {
			{x=pos.x  , y=pos.y  , z=pos.z-2},
			{x=pos.x-1, y=pos.y  , z=pos.z-1},
			{x=pos.x+1, y=pos.y  , z=pos.z-1},
			{x=pos.x  , y=pos.y-1, z=pos.z-1},
			{x=pos.x  , y=pos.y+1, z=pos.z-1},
			{x=pos.x-1, y=pos.y  , z=pos.z  },
			{x=pos.x+1, y=pos.y  , z=pos.z  },
			{x=pos.x  , y=pos.y-1, z=pos.z  },
			{x=pos.x  , y=pos.y+1, z=pos.z  },
			{x=pos.x-1, y=pos.y  , z=pos.z+1},
			{x=pos.x+1, y=pos.y  , z=pos.z+1},
			{x=pos.x  , y=pos.y-1, z=pos.z+1},
			{x=pos.x  , y=pos.y+1, z=pos.z+1},
			{x=pos.x  , y=pos.y  , z=pos.z+2}
		};
	elseif (type == 3) then
		for y=-1,1 do
			for z=-1,1 do
				table.insert(neighbors, {x=pos.x-2, y=pos.y+y, z=pos.z+z});
				table.insert(neighbors, {x=pos.x+2, y=pos.y+y, z=pos.z+z});
			end
		end

		for x=-1,1 do
			for z=-1,1 do
				table.insert(neighbors, {x=pos.x+x, y=pos.y-2, z=pos.z+z});
				table.insert(neighbors, {x=pos.x+x, y=pos.y+2, z=pos.z+z});
			end
		end

		for x=-1,1 do
			for y=-1,1 do
				table.insert(neighbors, {x=pos.x+x, y=pos.y+y, z=pos.z-2});
				table.insert(neighbors, {x=pos.x+x, y=pos.y+y, z=pos.z+2});
			end
		end
	end

	for _,p in ipairs(neighbors) do
		p.hidden = is_hidden(p);
	end

	ret = mod.next.block_action_rm(pos, type, from);

	for _,p in ipairs(neighbors) do
		if (p.hidden) then
			-- TODO: ick
			-- TODO: also, drop the set_?
			-- TODO: map_block -> voxel?
			send_set_block_color(PID_BROADCAST, get_map_block_color(p), get_anon_pid());
			send_block_action(PID_BROADCAST, p, 0, get_anon_pid());
		end
	end

	return ret;
end

return mod;
