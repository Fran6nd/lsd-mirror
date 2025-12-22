-- tip_spam.lua -- Occasionally spam all connected players with useless tips
local mod = {};
local next_tip_spam = nil;
local tip_spam_idx = 1;

function mod.tick()
	if (next_tip_spam == nil) then
		next_tip_spam = get_time() + tip_frequency; -- TODO: why did i comment out the + tip_frequency
	end

	--server.tick();
	next_call("tick", mod.tick)();
	--default.tick();

	if (get_time() >= next_tip_spam) then
		send_chat(PID_BROADCAST, tips[tip_spam_idx], 2, 0);

		tip_spam_idx = tip_spam_idx + 1;
		if (tip_spam_idx > #tips) then
			tip_spam_idx = 1;
		end

		next_tip_spam = next_tip_spam + tip_frequency;
	end
end

return mod;
-- for x,y in pairs(callchain) do
-- 	print(x,y);
-- 	for z,w in ipairs(y) do
-- 		print(z, w);
-- 	end
-- end
--
-- unregister(mod);
-- for x,y in pairs(callchain) do
-- 	print(x,y);
-- 	for z,w in ipairs(y) do
-- 		print(z, w);
-- 	end
-- end
