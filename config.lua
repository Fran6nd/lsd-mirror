package.path = "./scripts/?.lua"
-- TODO: from C
PID_BROADCAST = 32

-- Of course, all of this can be overridden by scripts, right?
name = "[LS2] democracy24"
team_name = {"bread", "cowboys"}
team_color = {
	{r=0, g=32, b=255},
	{r=255, g=32, b=0}
}
fog = {r=32, g=64, b=128}
max_score = 24;

-- local
callchain = {};
-- TODO: names?
modules = {};
local stexec = {};
function register(module)
	print("Loaded", module);
	table.insert(modules, module);
	for key, val in pairs(module) do
		if callchain[key] == nil then
			callchain[key] = {server[key]};
		end
		table.insert(callchain[key], val);
		_G[key] = function(...) status, err = pcall(val, ...); if (not status and err ~= stexec) then error(err); end end;
	end

	if (module.on_load ~= nil) then
		module.on_load();
	end
end

function unregister(module)
	local found = false;

	if (module.on_unload ~= nil) then
		module.on_unload();
	end

	for key, val in ipairs(modules) do
		if val == module then
			found = true;
			print("Unloaded", val);
			table.remove(modules, key);
			break;
		end
	end

	if (not found) then
		print("Couldn't unload", module);
		return;
	end

	for key, val in pairs(module) do
		for k, v in ipairs(callchain[key]) do
			if v == val then
				table.remove(callchain[key], k);
				_G[key] = function(...) status, err = pcall(callchain[key][#callchain[key]], ...); if (not status and err ~= stexec) then error(err); end end;
				--_G[key] = callchain[key][#callchain[key]];
				break;
			end
		end
	end
end

function next_call(funcname, func)
	local arr;
	for key, val in pairs(callchain) do
		if (key == funcname) then
			arr = val;
			break;
		end
	end

	for key, val in ipairs(arr) do
		if (val == func) then
			return arr[key-1];
		end
	end
end

function stop_exec()
	error(stexec);
end

function load(modname)
	mod = require(modname);
	if (type(mod) == "table") then
		register(mod);
	end
end

load "commands"
load "command_exec"
load "command_modutils"
load "command_cmds"
load "command_kill"

load "motd"
motd = [[
HIIIIIIIIIIIIIIIII!
THIS IS A MOTD
WHO COULD HAVE EXPECTED THAT
IT'S MADE IN LUA TOO
have i burned your ears off yet?
]]

load "tip_spam"
tips = {
	"This is a worthless tip.",
	"Did you learn something new today?",
	"Use /APOC to die.",
	"Press the L key to change teams (unless it's , or .)",
	"TODO: client-conditional tips",
	"Block color won't change? Try the arrow keys and E.",
	"This is not Build and Shoot. This is ACE OF SPADES.",
	"Some day I'll add a /tutor"
}
tip_frequency = 5*60

load "team_block_colors"
load "babel"
load "noclip"
load "jp"

--function randcolor()
--	return {r=math.random(0, 255), g=math.random(0, 255), b=math.random(0, 255)};
--end
