-- water_damage.lua -- The water is lava!!!
local mod = init_mod();
local damage_timer = pid_spawn_table(nil);

function mod.after.pyscrape_ext(scrape, str, meta)
	meta.water_damage = tonumber(scrape.get_ext(str, "water_damage"));
end

function mod.after.tick()
	local damage = get_map_meta().water_damage;
	if (damage == nil) then
		return;
	end

	local now = get_time();

	for i in piditer(PID_BROADCAST) do
		if (is_alive(i)) then
			if (damage_timer[i] == nil or now >= damage_timer[i]) then
				-- Remember that airborne players may be considered
				-- wading by the physics spaghetti-function!
				if (not is_airborne(i) and is_wading(i)) then
					local basetime = damage_timer[i] or now;
					damage_timer[i] = basetime + 1;

					damage_player(i, damage, 4, i);
				else
					damage_timer[i] = nil;
				end
			end
		end
	end
end

return mod;
