-- command_modutils.lua -- Manipulate loaded modules
-- TODO: remove command_ prefix?

-- TODO: set package.cpath
local function unreg(name)
	if (package.loaded[name]) then
		unregister(package.loaded[name]);
	end
	package.loaded[name] = nil;
end

local cmd = {name="unloadall", caps="modutils", fakepid=true, desc="Unregister all the modules."};
function cmd.func()
	local cmds = require("commands");

	for i=#modules,1,-1 do
		if (modules[i] ~= cmds) then
			unregister(modules[i]);
		end
	end
end
register_command(cmd, mod);

local cmd = {name="reloadall", caps="modutils", fakepid=true, usage="module", desc="Reload all registered modules."};
function cmd.func(pid, argv)
	local unregd = {};

	for i=#modules,1,-1 do
		unregd[i] = modules[i].name;
		if (unregd[i] == nil) then
			log("reloadall: %s has no name, cannot reload", tostring(modules[i]));
			unregister(modules[i]);
		else
			unreg(unregd[i]);
		end
	end

	for i=1,#unregd do
		if (unregd[i] ~= nil) then
			load(unregd[i]);
		end
	end
end
register_command(cmd, mod);

-- TODO: mod.cmds?
local cmd = {name="lsmod", caps="modutils", fakepid=true, desc="Crusty listing of all loaded modules."};
function cmd.func(pid)
	for x,y in ipairs(modules) do
		server_msg(pid, tostring(y.name or y));
	end
end
register_command(cmd, mod);

local cmd = {name="load", caps="modutils", fakepid=true, usage="module", desc="Dynamically load a module."};
function cmd.func(pid, argv)
	unreg(argv[1]);
	load(argv[1]);
end
register_command(cmd, mod);

local cmd = {name="unload", caps="modutils", fakepid=true, usage="module", desc="Dynamically unload a module."};
function cmd.func(pid, argv)
	unreg(argv[1]);
end
register_command(cmd, mod);

return {};
