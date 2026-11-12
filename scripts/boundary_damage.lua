-- boundary_damage.lua -- Punish players who step too far out of line
local mod = init_mod();
local damage_timer = pid_spawn_table(nil);

local function in_range(pid, tbl)
	local pos = get_position(pid);

	return
		pos.x >= tbl.left  and
		pos.x <= tbl.right and
		pos.y >= tbl.top   and
		pos.y <= tbl.bottom;
end

function mod.after.pyscrape_ext(scrape, str, meta)
	local str = scrape.get_ext(str, "boundary_damage");

	if (str == nil) then
		return;
	end

	-- TODO: maybe rewrite this to use lpeg if you don't hate yourself
	--       . . . just make sure it still parses that handful of broken
	--       maps i remember some time ago

	meta.boundary_damage = {
		left   = tonumber(str:match("['\"]left['\"].-:.-(%d+)")),
		right  = tonumber(str:match("['\"]right['\"].-:.-(%d+)")),
		top    = tonumber(str:match("['\"]top['\"].-:.-(%d+)")),
		bottom = tonumber(str:match("['\"]bottom['\"].-:.-(%d+)")),
		damage = tonumber(str:match("['\"]damage['\"].-:.-(%d+)"))
	};
end

function mod.after.tick()
	local tbl = get_map_meta().boundary_damage;
	if (tbl == nil) then
		return;
	end

	local now = get_time();

	for i in piditer(PID_BROADCAST) do
	if (
		is_alive(i) and
		(damage_timer[i] == nil or now >= damage_timer[i])
	) then
		if (not in_range(i, tbl)) then
			local basetime = damage_timer[i] or now;
			damage_timer[i] = basetime + 1;

			damage_player(i, tbl.damage, 4, i);
		else
			damage_timer[i] = nil;
		end
	end
	end
end

return mod;
