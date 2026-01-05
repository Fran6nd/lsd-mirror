-- command_modutils.lua -- Manipulate loaded modules
-- TODO: remove command_ prefix?

-- TODO: set package.cpath
local function unreg(name)
	if (package.loaded[name]) then
		unregister(package.loaded[name]);
	end
	package.loaded[name] = nil;
end

local cmd = {name="unloadall", caps="modutils", desc="Unregister all the modules."};
function cmd.func()
	local cmds = require("commands");

	for i=#modules,1,-1 do
		if (modules[i] ~= cmds) then
			unregister(modules[i]);
		end
	end
end
register_command(cmd);

-- TODO: mod.cmds?
local cmd = {name="lsmod", caps="modutils", desc="Crusty listing of all loaded modules."};
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

local cmd = {name="load", caps="modutils", usage="module", desc="Dynamically load a module."};
function cmd.func(pid, argv)
	unreg(argv[1]);
	load(argv[1]);
end
register_command(cmd);

local cmd = {name="unload", caps="modutils", usage="module", desc="Dynamically unload a module."};
function cmd.func(pid, argv)
	unreg(argv[1]);
end
register_command(cmd);
