-- sel.lua -- Perform bulk place/destroy operations on selections
require "lib_l10n";
require "lib_bulk_destroy";
local bit = require("bit");
local mod = init_mod();
local sel = pid_joined_table(nil);
local sel_shape = pid_joined_table("cube");
local sel_start = pid_joined_table(nil);
local sel_end = pid_joined_table(nil);

local sel_begin_msg = {
	en="Beginning selection."
};

local sel_begin_start_msg = {
	en="Beginning selection start."
};

local sel_begin_end_msg = {
	en="Beginning selection end."
};

local sel_start_done_msg = {
	en="Selection started at {x=%(x), y=%(y), z=%(z)}."
};

local sel_end_done_msg = {
	en="Selection ended at {x=%(x), y=%(y), z=%(z)}."
};

local sel_unbegan_msg = {
	en="Begin a selection first."
};

local sel_unstarted_msg = {
	en="Start the selection first."
};

local sel_unended_msg = {
	en="End the selection first."
};

-- TODO: automatically determine shapes
local invalid_shape_msg = {
	en="shape should be one of the following: cube, box, sphere, cylinderx, cylindery, cylinderz"
};

local invalid_dir_msg = {
	en="direction should be one of the following: x, -x, +x, y, -y, +y, z, -z, +z"
};

local cast_not_hit_msg = {
	en="Couldn't find any block in that cast. You're certain you're not looking at the sky?"
};

local shapes = {
	cube=true,
	box=true,
	sphere=true,
	cylinderx=true,
	cylindery=true,
	cylinderz=true
};

local function in_shape(pos, start, endp, shape)
	if (shape == "cube") then
		return true;
	end

	if (shape == "box") then
		return pos.x == start.x or pos.x == endp.x or
		       pos.y == start.y or pos.y == endp.y or
		       pos.z == start.z or pos.z == endp.z;
	end

	if (shape == "sphere" or string.find(shape, "^cylinder"))then
		local radius = {
			x=(endp.x-start.x)/2,
			y=(endp.y-start.y)/2,
			z=(endp.z-start.z)/2,
		};

		local ctr = {
			x=start.x+radius.x,
			y=start.y+radius.y,
			z=start.z+radius.z
		};

		local diff = {
			x=pos.x-ctr.x,
			y=pos.y-ctr.y,
			z=pos.z-ctr.z,
		};

		if (shape == "sphere") then
			return (diff.x*diff.x) / (radius.x*radius.x) + (diff.y*diff.y) / (radius.y*radius.y) + (diff.z*diff.z) / (radius.z*radius.z) <= 1;
		elseif (shape == "cylinderx") then
			return (diff.y*diff.y) / (radius.y*radius.y) + (diff.z*diff.z) / (radius.z*radius.z) <= 1;
		elseif (shape == "cylindery") then
			return (diff.x*diff.x) / (radius.x*radius.x) + (diff.z*diff.z) / (radius.z*radius.z) <= 1;
		else
			return (diff.x*diff.x) / (radius.x*radius.x) + (diff.y*diff.y) / (radius.y*radius.y) <= 1;
		end
	end
end

local cmd = {name="sel", caps="sel", desc="Select a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	sel[pid] = 0;
	sel_start[pid] = nil;
	sel_end[pid] = nil;
	l10n_send_chat(pid, sel_begin_msg);
end
register_command(cmd);

local cmd = {name={"selstart", "sel1"}, caps="sel", usage="[x y z]", desc="Set the start of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0 or #argv == 3);

	if (#argv == 0) then
		sel[pid] = -1;
		sel_start[pid] = nil;
		l10n_send_chat(pid, sel_begin_start_msg);
	else
		sel[pid] = nil;
		sel_start[pid] = {
			-- TODO: unhardcode map dimensions
			x=math.floor(get_arg_num_range("x", pid, cmd, argv[1], 0, 511)),
			y=math.floor(get_arg_num_range("y", pid, cmd, argv[2], 0, 511)),
			z=math.floor(get_arg_num_range("z", pid, cmd, argv[3], 0, 63))
		};
		l10n_send_chat(pid, sel_start_done_msg, sel_start[pid]);
	end
end
register_command(cmd);

local cmd = {name="sel1c", caps="sel", desc="Raycast the start of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	-- TODO: only not dead?
	-- TODO: make cast prettier?
	local pos = get_position(pid);
	local ori = get_orientation(pid);
	local castpos = raycast(pos, {x=pos.x+ori.x*512, y=pos.y+ori.y*512, z=pos.z+ori.z*512}, false);

	if (castpos == nil) then
		l10n_send_chat(pid, cast_not_hit_msg);
		return;
	end

	sel[pid] = nil;
	sel_start[pid] = castpos;
	l10n_send_chat(pid, sel_start_done_msg, sel_start[pid]);
end
register_command(cmd);

local cmd = {name="sel1h", caps="sel", desc="Set the start of a selection to your head's position."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	local pos = get_position(pid);
	pos.z = math.max(0, pos.z);

	sel[pid] = nil;
	sel_start[pid] = {x=math.floor(pos.x), y=math.floor(pos.y), z=math.floor(pos.z)};
	l10n_send_chat(pid, sel_start_done_msg, sel_start[pid]);
end
register_command(cmd);

local cmd = {name={"selend", "sel2"}, caps="sel", usage="[x y z]", desc="Set the end of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0 or #argv == 3);

	if (#argv == 0) then
		sel[pid] = 1;
		sel_end[pid] = nil;
		l10n_send_chat(pid, sel_begin_end_msg);
	else
		sel[pid] = nil;
		sel_end[pid] = {
			x=math.floor(get_arg_num_range("x", pid, cmd, argv[1], 0, 511)),
			y=math.floor(get_arg_num_range("y", pid, cmd, argv[2], 0, 511)),
			z=math.floor(get_arg_num_range("z", pid, cmd, argv[3], 0, 63))
		};
		l10n_send_chat(pid, sel_end_done_msg, sel_end[pid]);
	end
end
register_command(cmd);

local cmd = {name="sel2c", caps="sel", desc="Raycast the end of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	-- TODO: dedup sel1c/sel2c?
	local pos = get_position(pid);
	local ori = get_orientation(pid);
	local castpos = raycast(pos, {x=pos.x+ori.x*512, y=pos.y+ori.y*512, z=pos.z+ori.z*512}, false);

	if (castpos == nil) then
		l10n_send_chat(pid, cast_not_hit_msg);
		return;
	end

	sel[pid] = nil;
	sel_end[pid] = castpos;
	l10n_send_chat(pid, sel_end_done_msg, sel_end[pid]);
end
register_command(cmd);

local cmd = {name="sel2h", caps="sel", desc="Set the end of a selection to your head's position."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	local pos = get_position(pid);
	pos.z = math.max(0, pos.z);

	sel[pid] = nil;
	sel_end[pid] = {x=math.floor(pos.x), y=math.floor(pos.y), z=math.floor(pos.z)};
	l10n_send_chat(pid, sel_end_done_msg, sel_end[pid]);
end
register_command(cmd);

-- TODO: unsel -> selstop?
local cmd = {name="unsel", caps="sel", desc="Stop a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	sel[pid] = nil;
	sel_start[pid] = nil;
	sel_end[pid] = nil;
end
register_command(cmd);

local cmd = {name={"selshape", "selsh"}, caps="sel", usage="shape", desc="Change selection shape."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	if (shapes[argv[1]] == nil) then
		l10n_send_chat(pid, invalid_shape_msg);
		return;
	end

	sel_shape[pid] = argv[1];
end
register_command(cmd);

-- TODO: nuke PID_COLOR_ANONYMOUS
PID_COLOR_ANONYMOUS=31;

local function require_sel(pid)
	if (sel_start[pid] == nil or sel_end[pid] == nil) then
		if (sel[pid] == nil and sel_start[pid] == nil and sel_end[pid] == nil) then
			l10n_send_chat(pid, sel_unbegan_msg);
		elseif (sel_start[pid] == nil) then
			l10n_send_chat(pid, sel_unstarted_msg);
		else
			l10n_send_chat(pid, sel_unended_msg);
		end

		cmd_exit();
	end
end

local cmd = {name="selrep", caps="sel", desc="Fill and replace a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	set_block_color(PID_COLOR_ANONYMOUS, get_block_color(pid));
	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				local pos = {x=x, y=y, z=z};
				if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid])) then
					block_action(pos, 0, PID_COLOR_ANONYMOUS);
				end
			end
		end
	end
end
register_command(cmd);

local function linex(start, endp)
	for x=start.x,endp.x,50 do
		block_line({x=x, y=start.y, z=start.z}, {x=math.min(x+49, endp.x), y=endp.y, z=endp.z}, PID_COLOR_ANONYMOUS);
	end
end

local function liney(start, endp)
	for y=start.y,endp.y,50 do
		block_line({x=start.x, y=y, z=start.z}, {x=endp.x, y=math.min(y+49, endp.y), z=endp.z}, PID_COLOR_ANONYMOUS);
	end
end

local function linez(start, endp)
	for z=start.z,endp.z,50 do
		block_line({x=start.x, y=start.y, z=z}, {x=endp.x, y=endp.y, z=math.min(z+49, endp.z)}, PID_COLOR_ANONYMOUS);
	end
end

local function order(x1, x2)
	if (x1 <= x2) then
		return x1, x2;
	end

	return x2, x1;
end

local function iter_box(pid)
	local x1, x2 = order(sel_start[pid].x, sel_end[pid].x);
	local y1, y2 = order(sel_start[pid].y, sel_end[pid].y);
	local z1, z2 = order(sel_start[pid].z, sel_end[pid].z);

	for x=x1, x2, x1 > x2 and -1 or 1 do
		linez({x=x, y=y1, z=z1}, {x=x, y=y1, z=z2});
		linez({x=x, y=y2, z=z1}, {x=x, y=y2, z=z2});
	end

	for y=y1, y2, y1 > y2 and -1 or 1 do
		linex({x=x1, y=y, z=z1}, {x=x2, y=y, z=z1});
		linex({x=x1, y=y, z=z2}, {x=x2, y=y, z=z2});

		linez({x=x1, y=y, z=z1}, {x=x1, y=y, z=z2});
		linez({x=x2, y=y, z=z1}, {x=x2, y=y, z=z2});
	end
end

local cmd = {name="selfill", caps="sel", desc="Fill and do not replace a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	local x1, x2 = order(sel_start[pid].x, sel_end[pid].x);

	set_block_color(PID_COLOR_ANONYMOUS, get_block_color(pid));
	if (sel_shape[pid] == "cube") then
		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
				linex({x=x1, y=y, z=z}, {x=x2, y=y, z=z});
			end
		end
	elseif (sel_shape[pid] == "box") then
		iter_box(pid);
	else
		-- TODO: iter_sphere
		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
				for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
					local pos = {x=x, y=y, z=z};
					if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid]) and not is_solid(pos)) then
						block_action(pos, 0, PID_COLOR_ANONYMOUS);
					end
				end
			end
		end
	end
end
register_command(cmd);

local function do_rm(pid)
	-- TODO: range iter for single points, this is a mess
	if (sel_shape[pid] == "cube") then
		-- Destroy perimeter
		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
				bdestroy_block_action({x=sel_start[pid].x, y=y, z=z}, 1);
				bdestroy_block_action({x=sel_end[pid].x, y=y, z=z}, 1);
			end
		end

		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				bdestroy_block_action({x=x, y=sel_start[pid].y, z=z}, 1);
				bdestroy_block_action({x=x, y=sel_end[pid].y, z=z}, 1);
			end
		end

		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				bdestroy_block_action({x=x, y=y, z=sel_start[pid].z}, 1);
				bdestroy_block_action({x=x, y=y, z=sel_end[pid].z}, 1);
			end
		end
		bdestroy_finish();

		-- Clean up after floating blocks
		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
				for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
					local pos = {x=x, y=y, z=z};
					if (is_solid(pos)) then
						block_action(pos, 1, PID_COLOR_ANONYMOUS);
					end
				end
			end
		end

		return;
	end

	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				local pos = {x=x, y=y, z=z};
				if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid])) then
					bdestroy_block_action(pos, 1);
				end
			end
		end
	end
	bdestroy_finish();
end

-- TODO: integrate bulk operations into core, and do it smartly
local cmd = {name="selrm", caps="sel", desc="Destroy a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	do_rm(pid);
end
register_command(cmd);

-- TODO: handle voxlap
local cmd = {name="selpaint", caps="sel", desc="Set color of all blocks in a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	set_block_color(PID_COLOR_ANONYMOUS, get_block_color(pid));
	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				local pos = {x=x, y=y, z=z};
				if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid]) and is_solid(pos)) then
					block_action(pos, 0, PID_COLOR_ANONYMOUS);
				end
			end
		end
	end
end
register_command(cmd);

local dirmap = {
	["-x"]={x=-1, y= 0, z= 0},
	["+x"]={x= 1, y= 0, z= 0},
	[ "x"]={x= 1, y= 0, z= 0},

	["-y"]={x= 0, y=-1, z= 0},
	["+y"]={x= 0, y= 1, z= 0},
	[ "y"]={x= 0, y= 1, z= 0},

	["-z"]={x= 0, y= 0, z=-1},
	["+z"]={x= 0, y= 0, z= 1},
	[ "z"]={x= 0, y= 0, z= 1},
};

local function get_player_dir(pid)
	local ori = get_orientation(pid);

	if (math.abs(ori.x) >= math.abs(ori.y) and math.abs(ori.x) >= math.abs(ori.z)) then
		return {x=ori.x<0 and -1 or 1, y=0, z=0};
	elseif (math.abs(ori.y) >= math.abs(ori.z)) then
		return {x=0, y=ori.y<0 and -1 or 1, z=0};
	end

	return {x=0, y=0, z=ori.z<0 and -1 or 1};
end

local function do_selcpy(cmd, pid, argv, is_solid, get_map_block_color, forceoff)
	cmd_assert(pid, cmd, #argv <= 2);

	-- TODO: use -3 z instead of 3 -z?
	local dir;
	local times = get_arg_num_finite_opt("times", pid, cmd, argv[1]) or 1;

	if (#argv == 2) then
		dir = dirmap[argv[2]];
		if (dir == nil) then
			l10n_send_chat(pid, invalid_dir_msg);
			return;
		end
	else
		dir = get_player_dir(pid);
	end

	require_sel(pid);

	local x1, x2 = order(sel_start[pid].x, sel_end[pid].x);
	local y1, y2 = order(sel_start[pid].y, sel_end[pid].y);
	local z1, z2 = order(sel_start[pid].z, sel_end[pid].z);

	local off = {x=0, y=0, z=0};
	for i=1,times do
		local offpremult = {
			x=(x2-x1+1)*dir.x,
			y=(y2-y1+1)*dir.y,
			z=(z2-z1+1)*dir.z
		}

		if (forceoff == nil) then
			off = {
				x=offpremult.x*i,
				y=offpremult.y*i,
				z=offpremult.z*i
			};
		else
			off = forceoff;
		end

		if (x1 + off.x < 0 or
		    x2 + off.x >= 512 or
		    y1 + off.y < 0 or
		    y2 + off.y >= 512 or
		    z1 + off.z < 0 or
		    z2 + off.z >= 64) then
			off = {
				x=offpremult.x*(i-1),
				y=offpremult.y*(i-1),
				z=offpremult.z*(i-1)
			};
			break;
		end

		for z=z1,z2 do
			for y=y1,y2 do
				for x=x1,x2 do
					local pos = {x=x, y=y, z=z};
					local newpos = {x=x+off.x, y=y+off.y, z=z+off.z};

					if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid])) then
						if (not is_solid(pos)) then
							-- TODO: make destroy optional
							-- TODO: make sure this doesn't allow gravity to be "helpful"
							-- TODO: bring gravitied blocks back from the dead if you have to
							block_action(newpos, 1, PID_COLOR_ANONYMOUS);
						end
					end
				end
			end
		end

		for z=z1,z2 do
			for y=y1,y2 do
				for x=x1,x2 do
					local pos = {x=x, y=y, z=z};
					local newpos = {x=x+off.x, y=y+off.y, z=z+off.z};

					if (in_shape(pos, sel_start[pid], sel_end[pid], sel_shape[pid])) then
						if (is_solid(pos)) then
							set_block_color(PID_COLOR_ANONYMOUS, get_map_block_color(pos));
							block_action(newpos, 0, PID_COLOR_ANONYMOUS);
						end
					end
				end
			end
		end
	end

	local min = {
		x=math.min(x1+off.x, x1),
		y=math.min(y1+off.y, y1),
		z=math.min(z1+off.z, z1)
	}

	local max = {
		x=math.max(x2, x2+off.x),
		y=math.max(y2, y2+off.y),
		z=math.max(z2, z2+off.z)
	}

	return min, max;
end

-- TODO: /reselcpy to select whatever was just copied
local cmd = {name="selcpy", caps="sel", usage="[times] [direction]", desc="Duplicate the selection in a direction a certain number of times."};
function cmd.func(pid, argv)
	do_selcpy(cmd, pid, argv, is_solid, get_map_block_color);
end
register_command(cmd);

local cmd = {name="reselcpy", caps="sel", usage="[times] [direction]", desc="Duplicate the selection in a direction a certain number of times, then select the duplicated area."};
function cmd.func(pid, argv)
	sel_start[pid], sel_end[pid] = do_selcpy(cmd, pid, argv, is_solid, get_map_block_color);
end
register_command(cmd);

local function do_selmv(pid, cmd, off)
	require_sel(pid);

	local area = {};
	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				local pos = {x=x, y=y, z=z};
				if (is_solid(pos)) then
					local clr = get_map_block_color(pos);
					area[z+x*64+y*512*64] = bit.bor(clr.r, bit.bor(bit.lshift(clr.g, 8), bit.lshift(clr.b, 16)));
				end
			end
		end
	end

	do_rm(pid);

	-- TODO: bitmask api. . ???
	-- TODO: selclip/paste/save/load
	-- TODO: way to freeze arbitrary blocks in air?
	local function mv_is_solid(pos)
		return area[pos.z+pos.x*64+pos.y*512*64] ~= nil;
	end

	local function mv_get_map_block_color(pos)
		local ent = area[pos.z+pos.x*64+pos.y*512*64];
		return {r=bit.band(ent, 255), g=bit.band(bit.rshift(ent, 8), 255), b=bit.rshift(ent, 16)};
	end

	do_selcpy(cmd, pid, {1}, mv_is_solid, mv_get_map_block_color, off);

	-- Move players standing on selection
	local x1, x2 = order(sel_start[pid].x, sel_end[pid].x);
	local y1, y2 = order(sel_start[pid].y, sel_end[pid].y);
	local z1, z2 = order(sel_start[pid].z, sel_end[pid].z);

	for i in piditer(PID_BROADCAST) do
		-- TODO: conform to selshape?
		if (is_alive(i) and not is_airborne(i)) then
			local pos = get_position(i);
			if (
				pos.x >= x1 - 0.45 and
				pos.x < x2 + 1.45 and
				pos.y >= y1 - 0.45 and
				pos.y < y2 + 1.45 and

				pos.z >= z1 - 2.3 and
				pos.z < z2 - 0.3
			) then
				set_position(i, {x=pos.x+off.x, y=pos.y+off.y, z=pos.z+off.z});
			end
		end
	end

	sel_start[pid] = {
		x=sel_start[pid].x + off.x,
		y=sel_start[pid].y + off.y,
		z=sel_start[pid].z + off.z
	};

	sel_end[pid] = {
		x=sel_end[pid].x + off.x,
		y=sel_end[pid].y + off.y,
		z=sel_end[pid].z + off.z
	};
end

local cmd = {name="selmv", caps="sel", usage="x y z", desc="Move the selection, then select the moved area."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 3);

	-- TODO: modulo/validate??? what about z?
	local off = {
		x=math.floor(get_arg_num_finite("x", pid, cmd, argv[1])),
		y=math.floor(get_arg_num_finite("y", pid, cmd, argv[2])),
		z=math.floor(get_arg_num_finite("z", pid, cmd, argv[3]))
	};

	do_selmv(pid, cmd, off);
end
register_command(cmd);

local cmd = {name="selmvd", caps="sel", usage="dist", desc="Move the selection dist blocks in the direction you're facing, then select the moved area."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);
	local dist = get_arg_num_finite("dist", pid, cmd, argv[1]);

	-- TODO: modulo/validate??? what about z?
	local off = get_player_dir(pid);
	off = {
		x=off.x*dist,
		y=off.y*dist,
		z=off.z*dist
	};

	do_selmv(pid, cmd, off);
end
register_command(cmd);

function mod.on_block_action(pid, pos, type)
	if (type <= 1 and sel[pid]) then
		if (sel[pid] <= 0) then
			sel_start[pid] = pos;
			l10n_send_chat(pid, sel_start_done_msg, pos);

			if (sel[pid] == -1) then
				-- TODO: use 0 instead of nil?
				sel[pid] = nil;
			else
				sel[pid] = 1;
			end
		else
			sel_end[pid] = pos;
			l10n_send_chat(pid, sel_end_done_msg, pos);
			sel[pid] = nil;
		end
	else
		next_call("on_block_action", mod.on_block_action)(pid, pos, type);
	end
end

return mod;
