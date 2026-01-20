-- mapscripts.lua -- Execute arbitrary code and hopefully get a map out of it
local mod = {};

function mod.load_map_from_file(path)
	if (string.sub(path, -4, -1) == ".lua") then
		local oldpath = package.path;
		package.loaded.platforms=nil;

		-- TODO: dofile/loadfile instead of require? limit scope of visible functions?
		-- TODO: make load() use dofile instead of require?
		package.path="./?.lua";
		status, script = pcall(require, string.sub(path, 1, -5));
		package.path = oldpath;

		if (not status) then
			-- The 2nd value returned by pcall is err in this case
			error(script);
		end

		prepare_map_load();
		status, err = pcall(script.generate, math.random());
		finish_map_load();

		if (not status) then
			masterlist_set_map("???");
			error(err);
		end

		masterlist_set_map(string.match(path, "([^/]*).lua$"));

		return;
	end

	next_call("load_map_from_file", mod.load_map_from_file)(path);
end

return mod;
