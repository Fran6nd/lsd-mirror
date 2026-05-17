-- dd.lua -- The Dumbass Detector for servers
local mod = init_mod();

getcfg("dd_detect_kfc_hardaim", false);

-- These are all highly reliable (unlike pyspades RaPiD LAg detection),
-- *should* have few false positives and few false negatives (except for fast_dig_1x),
-- and only prevent cheats, instead of kicking over them. I recommend you
-- leave them on.
getcfg("dd_prevent_fast_block_place",      true);
getcfg("dd_prevent_fast_block_line_place", true);
getcfg("dd_prevent_fast_dig_1x",           true);
getcfg("dd_prevent_fast_dig_3x",           true);
-- This one's also rebranded as "multibullet" on occasion
getcfg("dd_prevent_fast_shoot",            true);
getcfg("dd_prevent_fast_spadehit",         true);

-- The freq value may need to be lowered (very gently!) to
-- account for shitty float32-summing timers on the other end
-- (used by OpenSpades, BetterSpades and probably Voxlap too) --
-- their accuracy decreases exponentially with playtime.
getcfg("dd_block_place_freq", 0.5);
getcfg("dd_dig_1x_freq",      0.2);
getcfg("dd_dig_3x_freq",      1);
getcfg("dd_shoot_freq",       {[0]=0.5, [1]=0.1, [2]=1});
getcfg("dd_spadehit_freq",    0.2);

-- Window size may need to be increased in case of variable round-trip time.
getcfg("dd_default_winsize",     0.25);
getcfg("dd_block_place_winsize", dd_default_winsize);
getcfg("dd_dig_1x_winsize",      dd_default_winsize);
getcfg("dd_dig_3x_winsize",      dd_default_winsize);
getcfg("dd_shoot_winsize",       {[0]=dd_default_winsize, [1]=dd_default_winsize, [2]=dd_default_winsize});
getcfg("dd_spadehit_winsize",    dd_default_winsize);

local function length(vec)
	return math.sqrt(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);
end

local function normalize(vec)
	local len = length(vec);
	if (len == 0) then
		return nil;
	end
	return {x=vec.x/len, y=vec.y/len, z=vec.z/len};
end

local function get_right(vec)
	local len = math.sqrt(vec.x*vec.x + vec.y*vec.y);
	return {x=-vec.y / len, y=vec.x / len, z=0};
end

local function cross(vec1, vec2)
	local out = {};

	out.x = vec1.y * vec2.z - vec1.z * vec2.y;
	out.y = vec1.z * vec2.x - vec1.x * vec2.z;
	out.z = vec1.x * vec2.y - vec1.y * vec2.x;

	return out;
end

local function sub(vec1, vec2)
	return {x=vec1.x-vec2.x, y=vec1.y-vec2.y, z=vec1.z-vec2.z};
end

local function mult31(vec1, num)
	return {x=vec1.x*num, y=vec1.y*num, z=vec1.z*num};
end

local function close_to_tol(val1, val2, tolerance)
	return math.abs(val1-val2) < tolerance;
end

function close_to(val1, val2)
	return close_to_tol(val1, val2, 0.00005);
end

local function destroy(pid)
	grant_cap(pid, "badcap:dd");
end

function mod.after.on_cap_grant(pid, cap)
	if (cap == "badcap:dd") then
		sc("!% "..tostring(pid).." is a big kaker");
	end
end

-- Head position, anyway.
-- TODO: use smooth position?
local function get_kentuckyfried_position(pos, ori)
	local vec = {x=pos.x, y=pos.y, z=pos.z};
	local right = get_right(ori);
	local down = cross(ori, right);

	vec.z = vec.z + 0.2;
	down = mult31(down, 0.3);
	vec = sub(vec, down);

	return vec;
end

-- TODO: there is a very specific ring that kfc softaim stops at, you can abuse that!

-- See if our friend is looking exactly at someone
-- TODO: check crap packets too
local function detect_kfc_hardaim(pid, pos)
	local ori = get_orientation(pid);
	if (ori.x == 0 or ori.y == 0 or ori.z == 0) then
		return;
	end

	-- Even dead people.
	for i in piditer(PID_BROADCAST_EXCEPT_TEAM(SPECTATOR)) do
		if (i == pid or not is_joined(i)) then goto continue; end

		local targetpos = get_kentuckyfried_position(get_position(i), get_orientation(i));
		local vec = {x=targetpos.x-pos.x, y=targetpos.y-pos.y, z=targetpos.z-pos.z};
		local veclen = length(vec);
		local testvec = {x=ori.x*veclen, y=ori.y*veclen, z=ori.z*veclen};

		if (vec ~= nil and close_to(vec.x, testvec.x) and close_to(vec.y, testvec.y) and close_to(vec.z, testvec.z)) then
			destroy(pid);
		end

		if (false and pid == 1 and vec ~= nil) then
			scl("(vec)"..fmtval(vec) .. " -- (testvec)" .. fmtval(testvec));
			scl(string.format("diff={x=%.6f, y=%.6f, z=%.6f}", vec.x-testvec.x, vec.y-testvec.y, vec.z-testvec.z));
		end

		::continue::
	end
end

function mod.after.on_position(pid, pos)
	if (dd_detect_kfc_hardaim) then
		detect_kfc_hardaim(pid, pos);
	end
end

local block_place_timer = pid_spawn_table(nil);
local dig_1x_timer = pid_spawn_table(nil);
-- TODO: handle canceling 3x by releasing rmb?
local dig_3x_timer = pid_spawn_table(nil);
local shoot_timer = pid_spawn_table(nil);
local spadehit_timer = pid_spawn_table(nil);

local function ratelimit(pid, timer, freq, winsiz, future)
	local now = get_time();

	if (timer[pid] == nil or now > timer[pid] + freq) then
		timer[pid] = now + (future and freq or 0);
	elseif (timer[pid] - now < winsiz) then
		timer[pid] = timer[pid] + freq;
	end

	return timer[pid] - now >= winsiz;
end

function mod.early.on_block_action(pid, pos, type)
	local tool = get_tool(pid);
	local gun = get_gun(pid);

	if     (type == 0 and dd_prevent_fast_block_place and ratelimit(pid, block_place_timer, dd_block_place_freq, dd_block_place_winsize)) then
		log("dd: prevented fast block placement from %s (#%u)", get_name(pid), pid);
		return;
	elseif (type == 1 and tool == 0 and dd_prevent_fast_dig_1x and ratelimit(pid, dig_1x_timer, dd_dig_1x_freq, dd_dig_1x_winsize)) then
		-- TODO: use science (protocol extensions?) to determine which blocks have been hit enough to be digged? [sic]
		log("dd: prevented fast 1x dig from %s (#%u)", get_name(pid), pid);
		return;
	elseif (type == 1 and tool == 2 and dd_prevent_fast_shoot and ratelimit(pid, shoot_timer, dd_shoot_freq[gun], dd_shoot_winsize[gun])) then
		log("dd: prevented fast block shot from %s (#%u)", get_name(pid), pid);
		return;
	elseif (type == 2 and dd_prevent_fast_dig_3x and ratelimit(pid, dig_3x_timer, dd_dig_3x_freq, dd_dig_3x_winsize)) then
		-- TODO: tie in mouse_input here and ditch the future arg?(??)
		log("dd: prevented fast 3x dig from %s (#%u)", get_name(pid), pid, true);
		return;
	end

	mod.early.next.on_block_action(pid, pos, type);
end

function mod.early.on_block_line(pid, startp, endp)
	if (dd_prevent_fast_block_line_place and ratelimit(pid, block_place_timer, dd_block_place_freq, dd_block_place_winsize)) then
		log("dd: prevented fast block line placement from %s (#%u)", get_name(pid), pid);
		return;
	end

	mod.early.next.on_block_line(pid, startp, endp);
end

function mod.early.on_hit(pid, type, hitPlayer)
	local tool = get_tool(pid);
	local gun = get_gun(pid);

	-- TODO: /an doesn't know it was denied
	if (tool == 0 and dd_prevent_fast_spadehit and ratelimit(pid, spadehit_timer, dd_spadehit_freq, dd_spadehit_winsize)) then
		log("dd: prevented fast spadehit from %s (#%u)", get_name(pid), pid);
		return;
	end

	if (tool == 2 and dd_prevent_fast_shoot and ratelimit(pid, shoot_timer, dd_shoot_freq[gun], dd_shoot_winsize[gun])) then
		log("dd: prevented fast player shot from %s (#%u)", get_name(pid), pid);
		return;
	end

	mod.early.next.on_hit(pid, type, hitPlayer);
end

return mod;
