-- core.lua -- Glue for other scripts
package.path = "./scripts/?.lua"
package.cpath = "./exec/?.so"

-- TODO: rip out most logging from the C core and implement it as a separate module
function color_to_ansi(str)
	local origsiz = #str;
	local teamcolor = {get_team_color(1), get_team_color(2)};

	str = string.gsub(str, "\r", "\n");
	str = string.gsub(str, "[\x0b\x0c\x0e-\x1f]", function(chr) return string.char(0x5e, string.byte(chr)+64); end);
	str = string.gsub(str, "\x01", "\x1b[38;2;"..teamcolor[1].r..";"..teamcolor[1].g..";"..teamcolor[1].b.."m");
	str = string.gsub(str, "\x02", "\x1b[38;2;"..teamcolor[2].r..";"..teamcolor[2].g..";"..teamcolor[2].b.."m");
	str = string.gsub(str, "\x03", "\x1b[35m");
	str = string.gsub(str, "\x04", "\x1b[31m");
	str = string.gsub(str, "\x05", "\x1b[32m");
	str = string.gsub(str, "\x06", "\x1b[0m");
	str = string.gsub(str, "\x07", "\x1b[90m");
	str = string.gsub(str, "\x08a", "\x1b[1m<RIFLE>\x1b[0m");
	str = string.gsub(str, "\x08b", "\x1b[1m<SMG>\x1b[0m");
	str = string.gsub(str, "\x08c", "\x1b[1m<SHOTGUN>\x1b[0m");
	str = string.gsub(str, "\x08d", "\x1b[1m<HEADSHOT>\x1b[0m");
	str = string.gsub(str, "\x08e", "\x1b[1m<SPADE>\x1b[0m");
	str = string.gsub(str, "\x08f", "\x1b[1m<GRENADE>\x1b[0m");
	str = string.gsub(str, "\x08g", "\x1b[1m<FALL>\x1b[0m");
	str = string.gsub(str, "\x08h", "\x1b[1m<TEAMSWITCH>\x1b[0m");
	str = string.gsub(str, "\x08i", "\x1b[1m<GUNSWITCH>\x1b[0m");
	str = string.gsub(str, "\x08j", "\x1b[1m<NOSCOPE>\x1b[0m");
	str = string.gsub(str, "\x08", "");

	if (#str ~= origsiz) then
		return str.."\x1b[0m";
	end

	return str;
end

function log(fmt, ...)
	io.stderr:write(color_to_ansi(string.format(fmt.."\n", ...)));
end
server.log = log;

function getcfg(key, default)
	if (_G[key] == nil) then
		_G[key] = default;
	end
end

callchain_impl = {};
callchain_late = {};
callchain_std = {};
callchain_early = {};

modules = {};

local status, err = pcall(math.randomseed);
if not status then
	log("Can't seed math.randomseed() with system entropy; are you sure this is what you want?");
	log("HINT: you're probably running regular Lua instead of LuaJIT");
end

local function process_before_after(tbl)
	if (tbl.before ~= nil) then
		for x,y in pairs(tbl.before) do
			if (tbl.after ~= nil and tbl.after[x] ~= nil) then
				local z = tbl.after[x];
				tbl[x] = function(...) y(...); local ret = tbl.next[x](...); z(...); return ret; end
				tbl.after[x] = nil;
			else
				tbl[x] = function(...) y(...); return tbl.next[x](...); end
			end
		end
	end

	if (tbl.after ~= nil) then
		for x,y in pairs(tbl.after) do
			tbl[x] = function(...) local ret = tbl.next[x](...); y(...); return ret; end
		end
	end

	tbl.before = nil;
	tbl.after = nil;
end

local function init_chains(name)
	-- TODO: hook callchain boundaries into core?
	callchain_impl[name] = {server[name]};
	callchain_late[name] = {function(...) return callchain_impl[name][#callchain_impl[name]](...); end};
	callchain_std[name] = {function(...) return callchain_late[name][#callchain_late[name]](...); end};
	callchain_early[name] = {function(...) return callchain_std[name][#callchain_std[name]](...); end};

	_G[name] = function(...)
		local status, err = pcall(callchain_early[name][#callchain_early[name]], ...);

		if (not status) then
			error(err, 2);
		end

		return err;
	end
end

local function add_cat(chain, tbl)
	if (tbl == nil) then
		return;
	end

	process_before_after(tbl);

	for name,func in pairs(tbl) do
		if (type(func) == "function") then
			if (chain[name] == nil) then
				init_chains(name);
			end

			tbl.next[name] = chain[name][#chain[name]];
			table.insert(chain[name], func);
		end
	end
end

local function get_func_owner(func, funcname, chainname)
	for _,mod in ipairs(modules) do
		local tbl = chainname and mod[chainname] or mod;

		if (tbl[funcname] == func) then
			return mod;
		end
	end
end

local function destroy_cat(chain, tbl, chainname)
	if (tbl == nil) then
		return;
	end

	-- Highly-nested code improves egg-laying performance
	for name,func in pairs(tbl) do
	if (type(func) == "function") then
	for i,chfunc in ipairs(chain[name]) do
	if (chfunc == func) then
		table.remove(chain[name], i);
		if (chain[name][i] ~= nil) then
			if (chainname) then
				get_func_owner(chain[name][i], name, chainname)[chainname].next[name] = chain[name][i-1];
			else
				get_func_owner(chain[name][i], name).next[name] = chain[name][i-1];
			end
		end
	end
	end
	end
	end
end
server.register = register;

function register(module)
	-- TODO: force modules to return tables
	log("Loaded %s", module.name or module);

	table.insert(modules, module);

	add_cat(callchain_impl, module.impl);
	add_cat(callchain_late, module.late);
	add_cat(callchain_std, module);
	add_cat(callchain_early, module.early);

	if (module.on_load ~= nil) then
		local status, err = pcall(module.on_load);
		if (not status) then
			unregister(module);
			log("on_load failed, unregistered module");
			error(err, 2);
		end
	end
end

function unregister(module, no_rm)
	local found = false;
	local status = true, err;

	if (module.on_unload ~= nil) then
		-- pcall on_unload to deal with errors preventing unregistration
		status, err = pcall(module.on_unload);
	end

	for key, val in ipairs(modules) do
		if (val == module) then
			found = true;

			if (not no_rm) then
				table.remove(modules, key);
			end

			break;
		end
	end

	if (not found) then
		log("Couldn't unload %s", module.name or module);
		return;
	end

	destroy_cat(callchain_impl, module.impl, "impl");
	destroy_cat(callchain_late, module.late, "late");
	destroy_cat(callchain_std, module);
	destroy_cat(callchain_early, module.early, "early");

	log("Unloaded %s", module.name or module);

	-- If module.unload threw an error, throw it again after it's unregistered
	if (not status) then
		error(err, 2);
	end
end
server.unregister = unregister;

-- TODO: remove dependency on require
function load(modname)
	mod = require(modname);
	if (type(mod) == "table") then
		mod.name = modname;
		register(mod);
	end
end

-- Just sets boilerplate
function init_mod()
	return {impl={next={}}, late={before={}, after={}, next={}}, early={before={}, after={}, next={}}, before={}, after={}, next={}};
end

-- Unregister everything on_shutdown -- most importantly this calls on_unload
local nextshutdown = on_shutdown;
function on_shutdown()
	for i=#modules,1,-1 do
		unregister(modules[i], true);
	end
	nextshutdown();
end
server.on_shutdown = on_shutdown;
