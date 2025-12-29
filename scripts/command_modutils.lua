-- command_modutils.lua -- Manipulate loaded modules

-- Don't ask.
local function unreg(name)
	mod = require(name);
	if (type(mod) == "table") then
		unregister(mod);
	end
	package.loaded[name] = nil;
end

local cmd = {name="unloadall", caps="modutils"};
function cmd.func()
	local cmds = require("commands");

	for i=#modules,1,-1 do
		if (modules[i] ~= cmds) then
			unregister(modules[i]);
		end
	end
end
register_command(cmd);

local cmd = {name="lsmod", caps="modutils"};
function cmd.func(pid)
	for x,y in ipairs(modules) do
		send_chat(pid, tostring(y), 2, 0);
		--print("mod", y);
		--for x,y in pairs(y) do
		--print(x, y);
		--end
	end
	-- for x,y in pairs(callchain) do
	-- 	print(x,y);
	-- 	for z,w in ipairs(y) do
	-- 		print(z, w);
	-- 	end
	-- end
end
register_command(cmd);

local cmd = {name="load", caps="modutils"};
function cmd.func(pid, argv)
	unreg(argv[1]);
	load(argv[1]);
end
register_command(cmd);

local cmd = {name="unload", caps="modutils"};
function cmd.func(pid, argv)
	unreg(argv[1]);
end
register_command(cmd);
