-- sel.lua -- Perform bulk place/destroy operations on selections
require "lib_l10n";
require "lib_bulk_destroy";
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
local sel_invalid_shape_msg = {
	en="shape should be one of the following: cube, box, sphere"
};

local shapes = {
	cube=true,
	box=true,
	sphere=true
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

	if (shape == "sphere") then
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

		return (diff.x*diff.x) / (radius.x*radius.x) + (diff.y*diff.y) / (radius.y*radius.y) + (diff.z*diff.z) / (radius.z*radius.z) <= 1;
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

local cmd = {name="selstart", caps="sel", desc="Set the start of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	sel[pid] = -1;
	sel_start[pid] = nil;
	l10n_send_chat(pid, sel_begin_start_msg);
end
register_command(cmd);

local cmd = {name="selend", caps="sel", desc="Set the end of a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	sel[pid] = 1;
	sel_end[pid] = nil;
	l10n_send_chat(pid, sel_begin_end_msg);
end
register_command(cmd);

local cmd = {name="unsel", caps="sel", desc="Stop a selection."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	sel[pid] = nil;
	sel_start[pid] = nil;
	sel_end[pid] = nil;
end
register_command(cmd);

local cmd = {name="selshape", caps="sel", usage="shape", desc="Change selection shape."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 1);

	if (shapes[argv[1]] == nil) then
		l10n_send_chat(pid, sel_invalid_shape_msg);
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

local function line(start, endp)
	for x=start.x,endp.x,50 do
		block_line({x=x, y=start.y, z=start.z}, {x=math.min(x+49, endp.x), y=endp.y, z=endp.z}, PID_COLOR_ANONYMOUS);
	end
end

local function iter_box(pid, func)
	for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
		func({x=x, y=sel_start[pid].y, z=sel_start[pid].z}, {x=x, y=sel_start[pid].y, z=sel_end[pid].z});
		func({x=x, y=sel_end[pid].y, z=sel_start[pid].z}, {x=x, y=sel_end[pid].y, z=sel_end[pid].z});
	end

	for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
		func({x=sel_start[pid].x, y=y, z=sel_start[pid].z}, {x=sel_end[pid].x, y=y, z=sel_start[pid].z});
		func({x=sel_start[pid].x, y=y, z=sel_end[pid].z}, {x=sel_end[pid].x, y=y, z=sel_end[pid].z});

		func({x=sel_start[pid].x, y=y, z=sel_start[pid].z}, {x=sel_start[pid].x, y=y, z=sel_end[pid].z});
		func({x=sel_end[pid].x, y=y, z=sel_start[pid].z}, {x=sel_end[pid].x, y=y, z=sel_end[pid].z});
	end
end

local cmd = {name="selfill", caps="sel", desc="Fill and do not replace a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	local x1, x2;

	if (sel_start[pid].x <= sel_end[pid].x) then
		x1, x2 = sel_start[pid].x, sel_end[pid].x;
	else
		x1, x2 = sel_end[pid].x, sel_start[pid].x;
	end

	set_block_color(PID_COLOR_ANONYMOUS, get_block_color(pid));
	if (sel_shape[pid] == "cube") then
		for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
			for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
				line({x=x1, y=y, z=z}, {x=x2, y=y, z=z});
			end
		end
	elseif (sel_shape[pid] == "box") then
		iter_box(pid, line);
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

-- TODO: integrate bulk operations into core, and do it smartly
local cmd = {name="selrm", caps="sel", desc="Destroy a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

	-- TODO: range iter for single points, this is a mess
	if (sel_shape[pid] == "cube") then
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
