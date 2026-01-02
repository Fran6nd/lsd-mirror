-- core.lua -- Glue for other scripts
package.path = "./scripts/?.lua"

-- TODO: make next_call work like pcall -- give it varargs
-- TODO: also maybe require the module to call a function to get its dedicated version of next_call, which already knows the module to look for
function log(fmt, ...)
	io.stderr:write(string.format(fmt.."\n", ...));
end

function getcfg(key, default)
	if (_G[key] == nil) then
		_G[key] = default;
	end
end

local stexec = {};
local function append_callchain(key, val)
	if callchain[key] == nil then
		-- TODO: does this need to reach into server, or will _G do fine?
		callchain[key] = {server[key]};
	end
	table.insert(callchain[key], val);
	_G[key] = function(...) local status, err = pcall(val, ...); if (not status and err ~= stexec) then error(err); end return err; end;
end

-- local
callchain = {};
-- TODO: names?
modules = {};
function register(module)
	log("Loaded %s", module);
	table.insert(modules, module);
	if (module.before ~= nil) then
		for x,y in pairs(module.before) do
			local patch;
			patch = function(...) y(...); next_call(x, patch)(...); end
			-- TODO: do you think overwriting things in the module will screw things up?
			-- TODO: especially if one mod registers both a before and an after
			module[x] = patch;
		end
	end
	if (module.after ~= nil) then
		for x,y in pairs(module.after) do
			local patch;
			patch = function(...) next_call(x, patch)(...); y(...); end
			module[x] = patch;
		end
	end
	module.before = nil;
	module.after = nil;
	for key, val in pairs(module) do
		append_callchain(key, val);
	end

	if (module.on_load ~= nil) then
		local status, err = pcall(module.on_load);
		if (not status) then
			log("on_load failed, unregistering module");
			unregister(module);
			error(err);
		end
	end
end

function unregister(module)
	local found = false;
	local status = true, err;

	if (module.on_unload ~= nil) then
		-- pcall on_unload to deal with errors preventing unregistration
		status, err = pcall(module.on_unload);
	end

	for key, val in ipairs(modules) do
		if val == module then
			found = true;
			log("Unloaded %s", val);
			table.remove(modules, key);
			break;
		end
	end

	if (not found) then
		log("Couldn't unload %s", module);
		return;
	end

	for key, val in pairs(module) do
		if (key == "before" or key == "after") then
			goto continue;
		end
		for k, v in ipairs(callchain[key]) do
			if v == val then
				table.remove(callchain[key], k);
				--_G[key] = function(...) status, err = pcall(callchain[key][#callchain[key]], ...); if (not status and err ~= stexec) then error(err); end end;
				_G[key] = function(...) local status, err = pcall(callchain[key][#callchain[key]], ...); if (not status and err ~= stexec) then error(err); end return err; end;
				--_G[key] = callchain[key][#callchain[key]];
				break;
			end
		end
		::continue::
	end

	-- If module.unload threw an error, throw it again after it's unregistered
	if (not status) then
		error(err);
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
