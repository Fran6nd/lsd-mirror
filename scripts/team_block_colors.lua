-- team_block_colors.lua -- Remove the players' freedom of choice for block colors
local mod = init_mod();

-- TODO: remove superfluous packet sending from core
-- TODO: or just send color packet to the player
function mod.on_color_change(pid, color)
	set_block_color(pid, get_team_color(get_team(pid)));
end

-- TODO: should i send a color packet right before block/line to keep things in sync?
-- if so, that should really be part of core
function mod.after.spawn_player(pid)
	-- TODO: should set_block_color ignore if there's no change in color?
	-- TODO: yes
	-- TODO: but what about the random dead person who places blocks?
	-- TODO: he's not dead to the server if he's doing that
	-- TODO: but still, dead people could have script-triggered block places -- shouldn't ignore dead people without user action

	-- ignore spectators
	if (get_team(pid) ~= SPECTATOR) then
		-- TODO: should there be a dedicated get_player_color func? don't make it too easy to confuse with block color though
		set_block_color(pid, get_team_color(get_team(pid)));
	end
end

return mod;
