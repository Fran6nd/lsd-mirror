-- map_meta.lua -- Extract metadata from maps without executing arbitrary code
-- TODO: optional arbitrary code execution?
local mod = {before={},after={}};
require "lib_l10n";
local scraper = require "lib_pyscrape";
local meta = {};

local next_up_msg_noauthor = {
	en="Next up: %(name)"
};

local next_up_msg_author = {
	en="Next up: %(name) by %(author)"
};

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
		local str;

		for i in piditer(PID_BROADCAST) do
			if (meta.author) then
				str = l10n_get_str_pid(PID_BROADCAST, next_up_msg_author, {name=meta.name, author=meta.author});
			else
				str = l10n_get_str_pid(PID_BROADCAST, next_up_msg_noauthor, {name=meta.name});
			end

			-- TODO: thanks to betterspades for making a 2nd standard that i have to support
			send_chat(PID_BROADCAST, "N% "..str, 2, 0);
		end
	end

	file:close();
end

function mod.after.load_map_from_file(path)
	if (meta.name) then
		-- TODO: truncate in lua.c and add potential for length extension?
		masterlist_set_map(string.sub(meta.name, 1, 20));
	end
end

-- TODO: don't depend on fmtval for this
local cmd = {name={"mapinfo", "mapname"}, fakepid=true, desc="Dump the current map's metadata."};
function cmd.func(pid)
	send_chat(pid, fmtval(meta), 2, 0);
end
register_command(cmd);

return mod;
