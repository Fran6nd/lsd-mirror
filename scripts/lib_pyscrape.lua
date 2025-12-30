-- lib_pyscrape.lua -- Attempt to scrape meaningful information from pyspades sidecar scripts without a real python interpreter
local mod = {};

-- TODO: should i use .* as the string instead of [^"']* and just search until the end?
-- TODO: handle escapes, specifically \'
local function getstr(str, name)
	return string.match(str, "[\r\n]"..name.."%s*=%s*[\"']([^\"']*)[\"']");
end

local function getfog(str, name)
	local r, g, b = string.match(str, "[\r\n]"..name.."%s*=%s*%(%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)%s*,?%s*%)");
	if (r) then
		return {r=tonumber(r), g=tonumber(g), b=tonumber(b)};
	end
end

local function scrape_spawn_locations(str, name)
	-- TODO: should the other ones match with a %b too?
	local start, endoff = string.find(str, "[\r\n]"..name.."%s*=%s*%b[]");
	local locs = {};

	if (start == nil) then
		return;
	end

	for x, y, z in string.gmatch(string.sub(str, start, endoff), "%(%s*(%d+)%s*,%s*(%d+)%s*,?%s*(%d*)%s*,?%s*%)") do
		-- TODO: do i need to account for "magic numbers"?
		table.insert(locs, {x=x+0.5, y=y+0.5, z=z});
	end

	return locs;
end

-- You may find this fun:
-- cut -c 1- aosParty/*.txt | grep = | cut -d = -f 1 | sed '/^[ \t#]/d' | tr -d ' ' | sort | uniq
function mod.grep(str)
	local meta = {};

	-- TODO: read line by line?
	str = "\n"..str.."\n";

	-- "nname" is a more popular mistake than you'd think
	meta.name = getstr(str, "n?[Nn]?ame");
	meta.version = getstr(str, "[Vv]ersion");
	meta.author = getstr(str, "[Aa]uthor");
	meta.description = getstr(str, "[Dd]escription");
	if (meta.description == nil) then
		meta.description = getstr(str, "[Dd]esc");
	end
	meta.fog = getfog(str, "[Ff]og");
	meta.spawn_locations_blue = scrape_spawn_locations(str, "spawn_locations_blue");
	meta.spawn_locations_green = scrape_spawn_locations(str, "spawn_locations_green");

	-- TODO: authors array?

	-- TODO: more spawn locations, intel/tent loc
	-- TODO: condense into one true spawn location format

	return meta;
end

return mod;
