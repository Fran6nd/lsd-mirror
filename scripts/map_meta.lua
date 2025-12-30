-- map_meta.lua -- Extract metadata from maps without executing arbitrary code
-- TODO: optional arbitrary code execution?
local mod = {before={}};
local scraper = require "lib_pyscrape";
local meta = {};

function mod.on_load()
	-- TODO: autoload metadata?
	meta = {};
end

-- TODO: /load of this here "before" doesn't quite unregister the old one
-- TODO: plumb metadata into core masterlist
function mod.before.load_map_from_file(path)
	local file = io.open(string.gsub(path, "%.vxl$", "", 1)..".txt", "rt");
	if (file == nil) then
		meta = {};
		return;
	end

	meta = scraper.grep(file:read("*a"));

	-- TODO: do i need to validate/clamp any of this?
	if (meta.fog) then
		set_fog(meta.fog);
	else
		set_fog(fog);
	end

	if (meta.name) then
		local authorstr = "";

		if (meta.author) then
			authorstr = " by " .. meta.author;
		end

		-- TODO: thanks to betterspades for making a 2nd standard that i have to support
		send_chat(PID_BROADCAST, "N% Next up: " .. meta.name .. authorstr, 2, 0);
	end

	file:close();
end

-- TODO: don't depend on fmtval for this
local cmd = {name={"mapinfo", "mapname"}};
function cmd.func(pid)
	send_chat(pid, fmtval(meta), 2, 0);
end
register_command(cmd);

return mod;
