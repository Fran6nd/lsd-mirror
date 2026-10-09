-- core.lua -- Glue for other scripts
package.path = "./scripts/?.lua"
package.cpath = "./exec/?.so"

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
server.color_to_ansi = color_to_ansi;

function log(fmt, ...)
	io.stderr:write(color_to_ansi(string.format(fmt.."\n", ...)));
end
server.log = log;

function getcfg(key, default)
	if (_G[key] == nil) then
		_G[key] = default;
	end
end

chains = {
	{}, -- impl
	{}, -- late
	{}, -- "std"
	{}, -- early
	{}  -- xearly
};

modules = {};

local status, err = pcall(math.randomseed);
if not status then
	log("Can't seed math.randomseed() with system entropy; are you sure this is what you want?");
	log("HINT: you're probably running regular Lua instead of LuaJIT");
end

local function getfn(next, name)
	local fn = next[name];

	if (type(fn) ~= "function") then
		error(string.format("%s.after.%s: no next in chain", tnam, name));
	end

	return fn;
end

local function process_before_after(tbl)
	local next = tbl.next;
	local tnam = tbl.name;

	for name, bfunc in pairs(tbl.before) do
		if (tbl.after[name] ~= nil) then
			local afunc = tbl.after[name];

			tbl[name] = function(...)
				bfunc(...);
				local ret = getfn(next, name)(...);
				afunc(...);
				return ret;
			end

			tbl.after[name] = nil;
		else
			tbl[name] = function(...)
				bfunc(...);
				return getfn(next, name)(...);
			end
		end
	end

	for name, afunc in pairs(tbl.after) do
		tbl[name] = function(...)
			local ret = getfn(next, name)(...);
			afunc(...);
			return ret;
		end
	end

	tbl.before = nil;
	tbl.after = nil;
end

local function relink_chains(name)
	local tbl = {[0]={}};

	for _, chain in ipairs(chains) do
		for _, chain_tbl in ipairs(chain[name]) do
			table.insert(tbl, chain_tbl);
		end
	end

	for i=#tbl,1,-1 do
		tbl[i].next[name] = tbl[i - 1][name];
	end

	_G[name] = tbl[#tbl][name];
end

local function init_chains(name)
	for _, chain in ipairs(chains) do
		chain[name] = {};
	end
end

local function add_cat(chain, tbl)
	if (tbl == nil) then
		return;
	end

	process_before_after(tbl);

	for name, func in pairs(tbl) do
	if (type(func) == "function") then
		if (chain[name] == nil) then
			init_chains(name);
		end

		table.insert(chain[name], tbl);
		relink_chains(name);
	end
	end
end

local function destroy_cat(chain, tbl)
	if (tbl == nil) then
		return;
	end

	for name, func in pairs(tbl) do
	if (type(func) == "function") then
		for i, chain_tbl in ipairs(chain[name]) do
		if (chain_tbl == tbl) then
			table.remove(chain[name], i);
			relink_chains(name);
			break;
		end
		end
	end
	end
end

local batch_loaded = 0;
function register(module)
	-- TODO: force modules to return tables
	if (batch_loaded ~= nil) then
		io.stderr:write((batch_loaded == 0 and "Loaded " or ", ")..(module.name or tostring(module)));
		batch_loaded = 1;
	else
		log("Loaded %s", module.name or module);
	end

	table.insert(modules, module);

	add_cat(chains[1], module.impl);
	add_cat(chains[2], module.late);
	add_cat(chains[3], module);
	add_cat(chains[4], module.early);
	add_cat(chains[5], module.xearly);

	if (module.on_load ~= nil) then
		local status, err = pcall(module.on_load);
		if (not status) then
			unregister(module);
			log("on_load failed, unregistered module");
			error(err, 2);
		end
	end
end
server.register = register;

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

	destroy_cat(chains[1], module.impl);
	destroy_cat(chains[2], module.late);
	destroy_cat(chains[3], module);
	destroy_cat(chains[4], module.early);
	destroy_cat(chains[5], module.xearly);

	if (not no_rm) then
		log("Unloaded %s", module.name or module);
	end

	-- If module.unload threw an error, throw it again after it's unregistered
	if (not status) then
		error(err, 2);
	end
end
server.unregister = unregister;

-- TODO: remove dependency on require
function load(modname)
	if (package.loaded[modname]) then
		unload(modname);
	end

	local mod = require(modname);
	if (type(mod) == "table") then
		mod.name = modname;
		register(mod);
	else
		package.loaded[modname] = nil;
	end
end

function unload(modname)
	unregister(package.loaded[modname] or {name=modname});
	package.loaded[modname] = nil;
end

-- Just sets boilerplate
function init_mod()
	return {
		impl   = {before={}, after={}, next={}},
		late   = {before={}, after={}, next={}},
		          before={}, after={}, next={} ,
		early  = {before={}, after={}, next={}},
		xearly = {before={}, after={}, next={}}
	};
end

-- Unregister everything on_shutdown -- most importantly this calls on_unload
local nextshutdown = on_shutdown;
function on_shutdown()
	for i=#modules,1,-1 do
		local status, err = pcall(unregister, modules[i], true);
		if (not status) then
			log("Error while unloading module at exit: %s", err);
		end
	end

	nextshutdown();
end
server.on_shutdown = on_shutdown;

local mod = init_mod();
function mod.before.load_initial_map()
	if (batch_loaded ~= 0) then
		io.stderr:write("\n");
	end

	batch_loaded = nil;
end

server.before = {};
server.after  = {};
server.next   = {};
server.name   = "server";

add_cat(chains[1], server); -- impl
add_cat(chains[3], mod);    -- "std"
