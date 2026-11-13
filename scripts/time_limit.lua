-- time_limit.lua -- Automatically advance the map just when the fun's beginning
local mod = init_mod();
local end_time = nil;
local announce_time = nil;
local explicit_announce_time = nil;

getcfg("time_limit", 2 * 3600);

-- If the remaining time is less than this value and an intel
-- is captured, the remaining time is set to this value.
getcfg("time_limit_capture_extend", 2 * 60);

-- Remaining time is regularly announced whenever this much time passes.
getcfg("time_limit_announce_interval", 30 * 60);

-- Remaining time is additionally announced whenever the amount
-- of time denoted by each key is remaining.
getcfg("time_limit_explicit_announce", {
	[10 * 60] = true,
	[60]      = true,
	[10]      = true
});

local extended_msg = {
	en="Remaining time extended to %(time)."
};

local remaining_msg = {
	en="%(time) remaining."
};

local need_cap_msg = {
	en="You need the %(cap) capability to set the time limit."
};

local function fmt_time()
	local remaining = math.ceil(end_time - get_time());

	if     (remaining <= 120) then
		return string.format("%i s", remaining);
	elseif (remaining <= 120 * 60) then
		-- %.0f rounds to nearest int
		return string.format("%.0f min", remaining / 60);
	elseif (remaining <= 120 * 3600) then
		return string.format("%.1f h", remaining / 3600);
	else
		return string.format("%.1f d", remaining / 86400);
	end
end

local function set_time(new_time)
	local now = get_time();

	end_time      = now + new_time;
	announce_time =
		now + time_limit_announce_interval -
		(new_time % time_limit_announce_interval);

	local new_announce = nil;
	for time, _ in pairs(time_limit_explicit_announce) do
		new_announce = new_announce or 0;
		new_announce = end_time - math.max(new_announce, time);
	end

	explicit_announce_time = now + new_announce;
end

function mod.after.capture_intel(pid)
	local now = get_time();

	if (end_time - now < time_limit_capture_extend) then
		set_time(time_limit_capture_extend);

		-- TODO: too chatty?
		l10n_send_chat(
			PID_BROADCAST,
			extended_msg,
			{time=fmt_time(time_limit_capture_extend)}
		);
	end
end

function mod.after.tick()
	local now = get_time();

	if (end_time == nil) then
		return;
	end

	if (now >= end_time) then
		advance_map();
		return;
	end

	if (explicit_announce_time and now >= explicit_announce_time) then
		l10n_send_chat(
			PID_BROADCAST,
			remaining_msg,
			{time=fmt_time(announce_time)}
		);

		local remaining = end_time - now;
		local next = 0;

		for time, _ in pairs(time_limit_explicit_announce) do
			if (time < remaining) then
				next = math.max(next, time);
			end
		end

		explicit_announce_time = end_time - next;

		-- fall-through
	end

	if (now >= announce_time) then
		l10n_send_chat(
			PID_BROADCAST,
			remaining_msg,
			{time=fmt_time(announce_time)}
		);

		announce_time = announce_time + time_limit_announce_interval;
		return;
	end
end

function mod.on_load()
	set_time(time_limit);
end

function mod.after.load_map()
	set_time(time_limit);
end

local cmd = {name={"time", "timelimit"}, fakepid=true, usage="[new_time]", desc="Print or set the time remaining until the game ends."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);

	if (#argv == 0) then
		l10n_send_chat(pid, remaining_msg, {time=fmt_time(get_time())});
		return;
	end

	if (not has_cap(pid, "time_limit")) then
		l10n_send_chat(pid, need_cap_msg, {cap="time_limit"});
		return;
	end

	set_time(get_arg_time("new_time", pid, cmd, argv[1]));
end
register_command(cmd, mod);

return mod;
