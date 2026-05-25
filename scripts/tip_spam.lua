-- tip_spam.lua -- Occasionally spam all connected players with useless tips
local mod = init_mod();
local next_tip_spam = nil;
local tip_spam_idx = 1;
-- TODO: randomly pick tips? (maybe with that shuffle method tetris uses)
-- TODO: comma/dot ordering
-- TODO: make wall scaling less annoying -- maybe depend on lastagreed and physics pos to be in a block? 
getcfg("tips", {
	function() for i in piditer(PID_BROADCAST) do server_msg(i, "Press the "..(get_client_char(i) == string.byte('o') and "L" or "comma/dot").." key to change team/gun."); end end,
	"Block color won't change? Try the arrow keys and E.",
	"Use the right mouse button to place multiple blocks at a time.",
	"Use /shutuptips if you're tired of getting tips. TODO: implement that and /tutor and maybe make /help an alias or associated",
	"Use /cmds to list commands.",
	"Build and Shoot is the name of the primary serverlist used for the game Ace of Spades as of writing this tip.",
	"You can scale walls faster by placing a block 3 blocks off the ground while midair and crouching.",
	"I hear gamebanana has some weapon skins -- just look for ones compatible with your client.",
	"There are 3-ish popular clients: original (\"Voxlap\", sometimes incorrectly referred to as \"buildandshoot\"), OpenSpades, and BetterSpades."
});
getcfg("tip_frequency", 5*60);

function mod.after.tick()
	if (next_tip_spam == nil) then
		next_tip_spam = get_time() + tip_frequency;
	end

	if (get_time() >= next_tip_spam) then
		local tip = tips[tip_spam_idx];

		if (type(tip) == "function") then
			local ret = tip();
			if (ret ~= nil) then
				server_msg(PID_BROADCAST, tostring(ret));
			end
		else
			server_msg(PID_BROADCAST, tip);
		end

		tip_spam_idx = tip_spam_idx + 1;
		if (tip_spam_idx > #tips) then
			tip_spam_idx = 1;
		end

		next_tip_spam = next_tip_spam + tip_frequency;
	end
end

return mod;
