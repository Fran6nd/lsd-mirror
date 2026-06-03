-- custom_restock.lua -- Tamper with the amount of hp/ammo a player's given on restock
local mod = init_mod();

getcfg("custom_restock_hp", 100);

-- If false, only alter mag ammo on spawn; if true, also set on restock
-- TODO: integrate with reloading probably
getcfg("custom_restock_set_mag", false);
getcfg("custom_restock_mag_ammo", {
	[0]=10,
	[1]=30,
	[2]=6
});

getcfg("custom_restock_reserve_ammo", {
	[0]=50,
	[1]=120,
	[2]=48
});

function mod.after.spawn_player(pid)
	if (custom_restock_hp ~= 100) then
		set_hp(pid, custom_restock_hp);
	end

	local gun = get_gun(pid);
	local newmag = custom_restock_mag_ammo[gun];
	local newres = custom_restock_reserve_ammo[gun];

	-- TODO: is it worth making this conditional at all?
	if (get_mag_ammo(pid) ~= newmag or get_reserve_ammo(pid) ~= newres) then
		set_ammo(pid, newmag, newres);
	end
end

function mod.after.restock(pid)
	if (custom_restock_hp ~= 100) then
		set_hp(pid, custom_restock_hp);
	end

	local gun = get_gun(pid);
	local newmag = custom_restock_set_mag and custom_restock_mag_ammo[gun] or get_mag_ammo(pid);
	local newres = custom_restock_reserve_ammo[gun];

	if (get_mag_ammo(pid) ~= newmag or get_reserve_ammo(pid) ~= newres) then
		set_ammo(pid, newmag, newres);
	end
end

return mod;
