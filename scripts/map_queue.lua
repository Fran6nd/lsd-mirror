-- map_queue.lua -- Pops the next map from a queue, and if the queue is empty picks a map from a different queue
local mod = init_mod();
local nextqueue = {};
-- TODO: list dirs in lua?
-- TODO: hook into initial map load
getcfg("map_queue", {
	"hallway",
	"hallweeb",
	"normandie",
	"war",
});
local map_queue_idx = 1;

local function tr_map_queue()
	if (type(map_queue) == "string") then
		local str = map_queue;
		map_queue = {};

		for x in string.gmatch(str, "[^\n\t]+") do table.insert(map_queue, x); end
	end
end

local function get_next_map()
	local map;

	if (#nextqueue > 0) then
		return table.remove(nextqueue);
	end

	-- TODO: support *clean* push/pull from map_queue -- should start at the map after the last played one, even if the last played one's index changes
	tr_map_queue();
	map = map_queue[map_queue_idx];

	map_queue_idx = map_queue_idx + 1;
	if (map_queue_idx > #map_queue) then
		map_queue_idx = 1;
	end

	return map;
end

-- TODO: on_server_start?
-- TODO: cleanup
-- TODO: should there be a hook for player disconnect/boot to limbo? on_unjoined?
-- TODO: should you combine the boot/load into one function?
-- TODO: what if the map is trash (not real/invalid/EOF)
function mod.on_game_end()
	load_map(get_next_map());
end

function mod.load_initial_map()
	load_map(get_next_map());
end

-- TODO: should it be legal to put a space or 30 before the command name? because we have that right now
-- TODO: how to specify a command takes no args?
-- TODO: should /showrotation be able to set rotation? (a la /mapqueue)
local cmd = {name={"showrotation", "mapqueue"}, fakepid=true, desc="List the default map queue."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	tr_map_queue();
	for _,y in ipairs(map_queue) do
		server_msg(pid, y);
	end
end
register_command(cmd, mod);

-- If you just want to let someone use e.g. /advance, give that someone the "cmd:advance" cap
local cmd = {name="queuemap", caps="map_queue", fakepid=true, usage="path", desc="Append a map to the temporary map queue."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	table.insert(nextqueue, argv[1]);
end
register_command(cmd, mod);

local cmd = {name={"setmapqueue", "map"}, caps="map_queue", fakepid=true, usage="path...", desc="Override the temporary map queue. Later maps get played first."};
function cmd.func(pid, argv)
	argv[0] = nil;
	nextqueue = argv;
end
register_command(cmd, mod);

-- TODO: on_game_end -> end_game?
local cmd = {name={"advance", "advancemap"}, caps="map_queue", fakepid=true, desc="Load the next queued map."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	on_game_end();
end
register_command(cmd, mod);

local cmd = {name="loadmap", caps="map_queue", fakepid=true, usage="path", desc="Immediately load the map at the specified path."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	table.insert(nextqueue, argv[1]);
	on_game_end();
end
register_command(cmd, mod);

return mod;
