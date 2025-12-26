-- medkit.lua -- Use /m for morphine
local mod = {};
local kits = {};
getcfg("medkit_heal", 40);
getcfg("medkit_quantity", 1);

function mod.on_load()
	for i=0,MAX_PLAYERS-1 do
		kits[i] = medkit_quantity;
	end
end

function mod.spawn_player(pid)
	next_call("spawn_player", mod.spawn_player)(pid);
	kits[pid] = medkit_quantity;
end

-- TODO: what about infinite_blocks restocks?
function mod.restock(pid)
	next_call("restock", mod.restock)(pid);
	kits[pid] = medkit_quantity;
end

local cmd = {name={"medkit", "m"}};
function cmd.func(pid)
	local hp;

	if (not is_alive(pid)) then
		-- TODO: wonder how the pyspades medkit reacts to dead men
		send_chat(pid, "The medkit cannot bring you back from the dead.", 2, 0);
		return;
	end

	if (kits[pid] == 0) then
		-- Of course, the restock part is only valid if tent restocking is enabled.
		send_chat(pid, "You've already used all your medkits! Restock at the tent.", 2, 0);
		return;
	end

	hp = get_hp(pid);
	if (hp >= 100) then
		send_chat(pid, "The medkit only heals physical wounds.", 2, 0);
		return;
	end

	kits[pid] = kits[pid] - 1;
	set_hp(pid, math.min(hp + medkit_heal, 100));
end
register_command(cmd);

return mod;
