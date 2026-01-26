-- uniqnams.lua -- Append trash to duplicate player names (TODO: too unixy of a name?)
local mod = init_mod();

-- piqueserver dropped this for some reason and made voxlap unhappy
-- TODO: reconsider/think this
local function deuce_id(pid, name)
	return name == "Deuce" and name..pid or name;
end

-- TODO: distinguish funcs that take a pid and funcs that take a piditer -- pid/pidi? easy to screw up though
local function iter_dup_name(pid, name)
	local iters = 1;
	local newname = name;

	-- TODO: you really need an iter for joined
	for i in piditer(PID_BROADCAST) do
		if (is_joined(i) and i ~= pid) then
			if (string.lower(get_name(i)) == string.lower(newname)) then
				newname = name..iters;
				iters = iters + 1;
			end
		end
	end

	return newname;
end

function mod.on_join(pid, team, weapon, name)
	local newname = iter_dup_name(pid, deuce_id(pid, name));

	-- Fall back to Deuce if the requested name is taken and the deduped version is too long
	if (#newname > 15) then
		newname = iter_dup_name(pid, deuce_id(pid, "Deuce"))
	end

	next_call("on_join", mod.on_join)(pid, team, weapon, newname);
end

return mod;
