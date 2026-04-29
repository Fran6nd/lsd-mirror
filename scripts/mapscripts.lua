-- mapscripts.lua -- Execute arbitrary code and hopefully get a map out of it
local mod = init_mod();

-- TODO: allow loading vxl with .lua ext? remove maps/?.lua req and just allow ?
function mod.load_map(name)
	if (string.sub(name, -4, -1) == ".lua") then
		local oldpath = package.path;
		package.loaded[string.sub(name, 1, -5)] = nil;

		-- TODO: dofile/loadfile instead of require? limit scope of visible functions?
		-- TODO: make load() use dofile instead of require?
		package.path="./?.lua";
		status, script = pcall(require, string.sub(name, 1, -5));
		package.path = oldpath;

		if (not status) then
			-- The 2nd value returned by pcall is err in this case
			error(script);
		end

		prepare_map_load();
		status, err = pcall(script.generate, math.random());
		math.randomseed();
		boot_players_to_limbo();
		finish_map_load();

		if (not status) then
			masterlist_set_map("???");
			error(err);
		end

		masterlist_set_map(string.match(name, "([^/]*).lua$"));

		return 0;
	end

	return mod.next.load_map(name);
end

return mod;
