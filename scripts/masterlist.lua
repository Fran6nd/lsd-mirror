-- masterlist.lua -- Interface (verb, TODO) with the server's masterlist implementation
local mod = init_mod();
local peermap = {};

getcfg("masterlist_name", "soupy server");
getcfg("masterlist_remotes", {
	-- Default port is 32886
	-- TODO: add scheme?
	"66.135.15.57", -- not a burner's secret masterlist
	--"master.buildandshoot.com"
});

local masterlist_connect_log_msg = {
	en="Connected to masterlist %(addr)"
};

local masterlist_disconnect_log_msg = {
	en="Disconnected from masterlist %(addr)"
};

local masterlist_reconnect_log_msg = {
	en="Reconnecting to masterlist %(addr)"
};

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

	-- TODO: sure hope nobody disconnects and reconnects a
	-- different peer without the peermap updating to compensate
	for _,y in ipairs(masterlist_remotes) do
		peermap[masterlist_connect(y, 32886)] = y;
	end
end

function mod.on_unload()
	for peer,_ in pairs(peermap) do
		masterlist_disconnect(peer);
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

function mod.after.on_masterlist_successful_connect(peerid)
	l10n_log(masterlist_connect_log_msg, {addr=peermap[peerid]});
end

function mod.after.on_masterlist_disconnect(peerid)
	l10n_log(masterlist_disconnect_log_msg, {addr=peermap[peerid]});
end

function mod.after.on_masterlist_reconnect_attempt(peerid)
	l10n_log(masterlist_reconnect_log_msg, {addr=peermap[peerid]});
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
