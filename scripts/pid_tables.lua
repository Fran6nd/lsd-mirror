-- pid_tables.lua -- Allow other scripts to create tables with pid keys that get automatically cleared under some condition
local mod = init_mod();
local clear_disconnect = {};
local clear_join = {};
local clear_spawn = {};

-- Make these tables weak so unused values get removed
setmetatable(clear_disconnect, {__mode="v"});
setmetatable(clear_join, {__mode="v"});
setmetatable(clear_spawn, {__mode="v"});

-- TODO: what if default is a table? we'd need to clone it. . .
local function init_pid_table(insertto, default, onclear)
	local tbl = {};

	for i=0,MAX_PLAYERS-1 do
		if (type(default) == "function") then
			tbl[i] = default(i);
		else
			tbl[i] = default;
		end
	end

	tbl.default = default;
	tbl.onclear = onclear;

	table.insert(insertto, tbl);
	return tbl;
end

-- TODO: can i use a global table for this?
-- Clears tbl[pid] when pid disconnects. There's a reason it does disconnect, TODO: put it here
function pid_connected_table(default)
	return init_pid_table(clear_disconnect, default);
end

-- Clears tbl[pid] when pid joins from limbo
function pid_joined_table(default)
	return init_pid_table(clear_join, default);
end

-- Clears tbl[pid] when pid spawns
function pid_spawn_table(default)
	return init_pid_table(clear_spawn, default);
end

local function clear_pid_table(pid, tbl)
	for _,ptbl in pairs(tbl) do
		if (ptbl.onclear) then
			ptbl.onclear(pid, ptbl);
		end

		if (type(ptbl.default) == "function") then
			ptbl[pid] = ptbl.default(pid);
		else
			ptbl[pid] = ptbl.default;
		end
	end
end

function mod.after.on_disconnect(pid)
	clear_pid_table(pid, clear_disconnect);
end

function mod.after.disconnect_now(pid)
	clear_pid_table(pid, clear_disconnect);
end

-- TODO: before?
function mod.after.on_join(pid)
	clear_pid_table(pid, clear_join);
end

function mod.after.spawn_player(pid)
	clear_pid_table(pid, clear_spawn);
end

return mod;
