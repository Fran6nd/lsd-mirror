-- sel.lua -- Perform bulk place/destroy operations on selections
require "lib_l10n";
require "lib_bulk_destroy";
local mod = init_mod();
local sel = pid_joined_table(nil);
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

-- TODO: no args
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
				block_action({x=x, y=y, z=z}, 0, PID_COLOR_ANONYMOUS);
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
	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			line({x=x1, y=y, z=z}, {x=x2, y=y, z=z});
		end
	end
end
register_command(cmd);

-- TODO: integrate bulk operations into core, and do it smartly
local cmd = {name="selrm", caps="sel", desc="Destroy a box."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);
	require_sel(pid);

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

	for z=sel_start[pid].z, sel_end[pid].z, sel_start[pid].z > sel_end[pid].z and -1 or 1 do
		for y=sel_start[pid].y, sel_end[pid].y, sel_start[pid].y > sel_end[pid].y and -1 or 1 do
			for x=sel_start[pid].x, sel_end[pid].x, sel_start[pid].x > sel_end[pid].x and -1 or 1 do
				bdestroy_block_action({x=x, y=y, z=z}, 1);
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
			sel[pid] = sel[pid] + 1;
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
