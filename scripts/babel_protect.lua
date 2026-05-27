-- babel_protect.lua -- Enable building indestructible walls on towers
local mod = init_mod();
local bit = require("bit");

-- babel_width, babel_height and babel_z are duplicated from babel.lua
getcfg("babel_width", 100);
getcfg("babel_height", 32);
getcfg("babel_z", 1);

getcfg("babel_protect_dash_modulo", 4)
getcfg("babel_protect_dash_width", 2)
getcfg("babel_protect_dash_pattern", 0)
getcfg("babel_protect_zones", {
	{
		{{x=128, y=256-babel_height/2}, {x=255-babel_width/2+20, y=255+babel_height/2}}
	},
	{
		{{x=256+babel_width/2-20, y=256-babel_height/2}, {x=383, y=255+babel_height/2}}
	}
});

local no_build_msg = {
	en="Can't do that there!",
};

local function get_highest_z(x, y)
	local start = 0;
	if (x >= 256-babel_width/2 and x <= 255+babel_width/2 and y >= 256-babel_height/2 and y <= 255+babel_height/2) then
		start = babel_z+1;
	end

	for z=start,62 do
		if (is_solid({x=x, y=y, z=z})) then
			return z;
		end
	end

	return 63;
end

local function is_dash(x, y)
	return (bit.bxor(x, y)+babel_protect_dash_pattern) % babel_protect_dash_modulo < babel_protect_dash_width;
end

local function paint_lines()
	for team=1,2 do
		set_block_color(PID_COLOR_ANONYMOUS, get_team_color(team));
		for _,zone in ipairs(babel_protect_zones[team]) do
			for x=zone[1].x,zone[2].x do
				if (is_dash(x, zone[1].y)) then
					block_action({x=x, y=zone[1].y, z=get_highest_z(x, zone[1].y)}, 0, PID_COLOR_ANONYMOUS);
				end
				if (is_dash(x, zone[2].y)) then
					block_action({x=x, y=zone[2].y, z=get_highest_z(x, zone[2].y)}, 0, PID_COLOR_ANONYMOUS);
				end
			end

			for y=zone[1].y,zone[2].y do
				if (is_dash(zone[1].x, y)) then
					block_action({x=zone[1].x, y=y, z=get_highest_z(zone[1].x, y)}, 0, PID_COLOR_ANONYMOUS);
				end
				if (is_dash(zone[2].x, y)) then
					block_action({x=zone[2].x, y=y, z=get_highest_z(zone[2].x, y)}, 0, PID_COLOR_ANONYMOUS);
				end
			end
		end
	end

	local zone = {{x=255-babel_width/2, y=256-babel_height/2}, {x=256+babel_width/2, y=255+babel_height/2}};

	-- Team color is borrowed from the prior incantations in this function
	for y=zone[1].y,zone[2].y do
		if (is_dash(zone[2].x, y)) then
			block_action({x=zone[2].x, y=y, z=get_highest_z(zone[2].x, y)}, 0, PID_COLOR_ANONYMOUS);
		end
	end

	set_block_color(PID_COLOR_ANONYMOUS, get_team_color(1));
	for y=zone[1].y,zone[2].y do
		if (is_dash(zone[1].x, y)) then
			block_action({x=zone[1].x, y=y, z=get_highest_z(zone[1].x, y)}, 0, PID_COLOR_ANONYMOUS);
		end
	end
end

function mod.on_load()
	paint_lines();
end

function mod.before.finish_map_load()
	paint_lines();
end

local function within(point, start, endp)
	return point >= start and point <= endp;
end

local function within_xy(pos, start, endp)
	return within(pos.x, start.x, endp.x) and within(pos.y, start.y, endp.y);
end

local function adjacent(point, start, endp)
	return point >= start - 1 and point <= endp + 1;
end

local function adjacent_xy(pos, start, endp)
	return adjacent(pos.x, start.x, endp.x) and adjacent(pos.y, start.y, endp.y);
end

local function legal_pos(pid, pos, type)
	local team = get_team(pid);
	local func = type <= 2 and within_xy or adjacent_xy;

	if (type ~= 0 and has_cap(pid, "no_babel_protect")) then
		return true;
	end

	team = type == 0 and (team == 1 and 2 or 1) or team;

	for _,zone in ipairs(babel_protect_zones[team]) do
		if (func(pos, zone[1], zone[2])) then
			return false;
		end
	end

	return true;
end

function mod.early.on_block_action(pid, pos, type)
	if (not legal_pos(pid, pos, type)) then
		l10n_send_chat(pid, no_build_msg);
		return;
	end

	mod.early.next.on_block_action(pid, pos, type);
end

-- TODO: make grenades call on_block_action()?
-- TODO: don't yell "Can't do that there!" for breaking 0 blocks with a midair nade?
function mod.early.block_action(pos, type, pid)
	if (pid >= 0 and pid < MAX_PLAYERS and is_joined(pid) and type == 3 and not legal_pos(pid, pos, type)) then
		l10n_send_chat(pid, no_build_msg);
		return;
	end

	mod.early.next.block_action(pos, type, pid);
end

function mod.early.on_block_line(pid, startp, endp)
	for pos in iter_block_line(startp, endp) do
		if (not legal_pos(pid, pos, 0)) then
			l10n_send_chat(pid, no_build_msg);
			return;
		end
	end

	mod.early.next.on_block_line(pid, startp, endp);
end

return mod;
