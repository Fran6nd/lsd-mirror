-- chatpat.lua -- Run funny pattern replacements over all chat messages and player names
local mod = init_mod();

-- Spacing chars
local s = "%c%p%s";

local function assemble_character(chars)
	if (chars == "i" or chars == "1" or chars == "l" or chars == "|" or chars == "e" or chars == "a" or chars == "@") then
		chars = "i1l|ea@";
	elseif (chars == "g" or chars == "q" or chars == "9" or chars == "6") then
		chars = "gq96";
	elseif (chars == "o" or chars == "0") then
		chars = "o0";
	elseif (chars == "c" or chars == "k") then
		chars = "ck";
	elseif (chars == "u" or chars == "v") then
		chars = "uv";
	end

	return chars .. string.upper(chars);
end

function chatpat_parse(str)
	local out = "";

	for i=1,#str,2 do
		local char = string.sub(str, i, i);

		if (char == "%" or char == "(" or char == ")") then
			out = out .. string.sub(str, i+1, i+1);
		else
			char = assemble_character(char);
			local modifier = string.sub(str, i+1, i+1);

			if (modifier ~= "*") then
				out = out .. "["..char.."]";
			end

			out = out .. "["..char..s.."]".."*";
		end
	end

	return out;
end

-- Improves the two most popular words by default. Add something fun, won't you?
getcfg("chatpat_chat", {
	{chatpat_parse("n+e+g+r+((o+))"), "potat%1"},
	{chatpat_parse("n+i+((g+))e*r+"), "di%1er"},
	{chatpat_parse("n+i+g+((%[%a%@%s% %]%+))"), "sop%1"},
	{chatpat_parse("n+i+g+((%[%a%@%s% %]%*))%$"), "sop%1"},
	{chatpat_parse("f+((a+))g+o*t*"), "m%1n"},
	--{chatpat_parse("f+((u+))k+"), "fl%1ff"},
	--{"onion", "garlic"}
});

getcfg("chatpat_name", {
	{chatpat_parse("n+e+g+r+((o+))"), "potat%1"},
	{chatpat_parse("n+i+((g+))e*r+"), "di%1er"},
	{chatpat_parse("n+i+g+((%[%a%@%s% %]%+))"), "sop%1"},
	{chatpat_parse("n+i+g+((%[%a%@%s% %]%*))%$"), "sop%1"},
	{chatpat_parse("f+((a+))g+o*t*"), "m%1n"},
	--{chatpat_parse("f+((u*))k+"), "fl%1ff"},
	--{"onion", "garlic"}
});

function mod.on_chat(pid, msg, type)
	for _,y in ipairs(chatpat_chat) do
		msg = string.gsub(msg, y[1], y[2]);
	end

	mod.next.on_chat(pid, msg, type);
end

function mod.early.on_join(pid, team, weapon, name)
	for _,y in ipairs(chatpat_name) do
		name = string.gsub(name, y[1], y[2]);
	end

	-- Fall back to Deuce if the new name is too long
	if (#name > 15) then
		name = "Deuce";
	end

	mod.early.next.on_join(pid, team, weapon, name);
end

return mod;
