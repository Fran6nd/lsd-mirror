-- medkit.lua -- Use /m for morphine
local mod = init_mod();
local kits = {};
getcfg("medkit_heal", 40);
getcfg("medkit_quantity", 1);

local dead_msg = {
	en="The medkit cannot bring you back from the dead."
};

local none_left_msg = {
	en="You've already used all your medkits! Restock at the tent."
};

local full_hp_msg = {
	en="The medkit only heals physical wounds."
};

function mod.on_load()
	for i in piditer(PID_BROADCAST) do
		kits[i] = medkit_quantity;
	end
end

function mod.after.spawn_player(pid)
	kits[pid] = medkit_quantity;
end

-- TODO: what about infinite_blocks restocks?
function mod.after.restock(pid)
	kits[pid] = medkit_quantity;
end

local cmd = {name={"medkit", "m"}};
function cmd.func(pid)
	local hp;

	if (not is_alive(pid)) then
		-- TODO: wonder how the pyspades medkit reacts to dead men
		l10n_send_chat(pid, dead_msg);
		return;
	end

	if (kits[pid] == 0) then
		-- Of course, the restock part is only valid if tent restocking is enabled.
		l10n_send_chat(pid, none_left_msg);
		return;
	end

	hp = get_hp(pid);
	if (hp >= 100) then
		l10n_send_chat(pid, full_hp_msg);
		return;
	end

	kits[pid] = kits[pid] - 1;
	set_hp(pid, math.min(hp + medkit_heal, 100));
end
register_command(cmd, mod);

return mod;
