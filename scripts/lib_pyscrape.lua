-- lib_pyscrape.lua -- Attempt to scrape meaningful information from pyspades sidecar scripts without a real python interpreter
local mod = {};

-- TODO: should i use .* as the string instead of [^"']* and just search until the end?
-- TODO: handle escapes, specifically \'
function mod.getstr(str, name)
	return string.match(str, "[\r\n]"..name.."%s*=%s*[\"']([^\"']*)[\"']");
end

function mod.get_ext(str, name)
	return string.match(str, "[\r\n]%s*[\"']"..name.."[\"']%s*:%s*([^\r\n]-)%s*,?%s*[\r\n]");
end

function mod.parse_str(str)
	return string.match(str, "[\"']([^\"']*)[\"']");
end

function mod.parse_bool(str)
	if (str == "False" or str == "None" or str == "0") then
		return false;
	end

	return true;
end

function mod.split_tuple(str)
	str = string.match(str, "%((.*)%)");
	if (not string.match(str, ",%s*$")) then
		str = str..",";
	end
	return string.gmatch(str, "%s*(.-)%s*,");
end

function mod.split_tupletuple(str)
        str = string.match(str, "%((.*)%)");
        if (not string.match(str, ",%s*$")) then
                str = str..",";
        end
        return string.gmatch(str, "%s*(%b())%s*,");
end

function mod.getfog(str, name)
	local r, g, b = string.match(str, "[\r\n]"..name.."%s*=%s*%(%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)%s*,?%s*%)");
	if (r) then
		return {r=tonumber(r), g=tonumber(g), b=tonumber(b)};
	end
end

function mod.scrape_spawn_locations(str, name)
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

-- TODO: migrate to real module
function pyscrape_ext(mod, str, meta)end
server.pyscrape_ext = pyscrape_ext;

-- You may find this fun:
-- cut -c 1- aosParty/*.txt | grep = | cut -d = -f 1 | sed '/^[ \t#]/d' | tr -d ' ' | sort | uniq
function mod.grep(str)
	local meta = {};

	-- TODO: read line by line?
	str = "\n"..str.."\n";

	-- "nname" is a more popular mistake than you'd think
	meta.name = mod.getstr(str, "n?[Nn]?ame");
	meta.version = mod.getstr(str, "[Vv]ersion");
	meta.author = mod.getstr(str, "[Aa]uthor");
	meta.description = mod.getstr(str, "[Dd]escription");
	if (meta.description == nil) then
		meta.description = mod.getstr(str, "[Dd]esc");
	end
	meta.fog = mod.getfog(str, "[Ff]og");
	meta.spawn_locations_blue = mod.scrape_spawn_locations(str, "spawn_locations_blue");
	meta.spawn_locations_green = mod.scrape_spawn_locations(str, "spawn_locations_green");

	pyscrape_ext(mod, str, meta);

	-- TODO: authors array?

	-- TODO: more spawn locations, intel/tent loc
	-- TODO: condense into one true spawn location format

	return meta;
end

return mod;
