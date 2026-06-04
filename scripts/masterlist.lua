-- masterlist.lua -- Interface (verb, TODO) with the server's masterlist implementation
local mod = init_mod();
getcfg("masterlist_name", "soupy server");
getcfg("masterlist_remotes", {
	-- Default port is 32886
	-- TODO: add scheme?
	"66.135.15.57", -- not a burner's secret masterlist
	--"master.buildandshoot.com"
});

local function calc_players()
	local players = 0;
	local max = get_effective_max_players();

	for i in piditer(PID_BROADCAST) do
		if (is_joined(i)) then
			players = players + 1;
		else
			max = max - 1;
		end
	end

	masterlist_set_players(players);
	masterlist_set_max_players(max);
end

function mod.on_load()
	masterlist_set_name(masterlist_name);
	calc_players();

	for _,y in ipairs(masterlist_remotes) do
		masterlist_connect(y);
	end
end

function mod.after.on_successful_connect()
	calc_players();
end

function mod.after.on_join()
	calc_players();
end

function mod.after.boot_players_to_limbo()
	calc_players();
end

function mod.after.on_disconnect()
	calc_players();
end

function mod.after.disconnect_now()
	calc_players();
end

-- TODO: actually connect to masterlist in this script, not hardcoded to magicserver
-- TODO: move name into here from lua.c, and actually do something with it
function mod.after.load_map(path)
	-- TODO: .lua? need to make the custom loaders canonicalize the name for me. . .
	local name = string.match(path, "([^/]*).vxl$");
	if (name == nil) then
		name = string.match(path, "([^/]*)$");
	end
	masterlist_set_map(name);
end

return mod;
