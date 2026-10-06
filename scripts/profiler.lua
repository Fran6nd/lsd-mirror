-- profiler.lua -- A little percussive maintenance 'ought to do the trick
local mod = init_mod();
local profile = require("jit.profile");

local lines;
local function callback(thread, samples, vmstate)
	local line = profile.dumpstack(thread, "l", 1);
	lines[line] = (lines[line] or 0) + 1;
end

function mod.impl.profiler_start()
	lines = {};
	profile.start("l", callback);
end

function mod.impl.profiler_stop()
	profile.stop();
end

function mod.impl.profiler_dump()
	local sorted = {};

	for k,v in pairs(lines) do
		table.insert(sorted, {v, k});
	end

	table.sort(sorted, function(a, b) return a[1] < b[1]; end);

	for _,v in ipairs(sorted) do
		log("%s\t%s", v[1], v[2]);
	end
end

local cmd = {name={"profiler_start", "pfstart"}, caps="profiler", fakepid=true, desc="Start profiling."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	profiler_start();
end
register_command(cmd, mod);

local cmd = {name={"profiler_stop", "pfstop"}, caps="profiler", fakepid=true, desc="Stop profiling."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	profiler_stop();
end
register_command(cmd, mod);

local cmd = {name={"profiler_dump", "pfdump"}, caps="profiler", fakepid=true, desc="Dump profiler data to the console."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	profiler_dump();
end
register_command(cmd, mod);

return mod;
