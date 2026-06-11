-- emote.lua -- Paint dank memes on the ground
local mod = init_mod();
local ffi = require("ffi");
local lfs = require("lfs");
local stbi = require("lib_stbimage");

local timeouttimer = pid_joined2_table(0);

local origerr = nil;
local img = pid_joined2_table(nil);
local placepos = pid_joined2_table(nil);
local placeoff = pid_joined2_table(nil);
local placetimer = pid_joined2_table(nil);

getcfg("emote_basedir", "emote/");
getcfg("emote_suffix", ".png");
-- Probably don't make the emotes bigger than the timeout
-- if you don't successive emotes to be canceled :p
getcfg("emote_timeout", 20);
-- Set to false if you want paint to be client-side only
getcfg("emote_sync", true);

local bad_image_msg = {
	en="Cannot find image; try /emotelist."
};

local use_emotelist_msg = {
	en="You can list images with /emotelist."
};

local timeout_msg = {
	en="You must wait %(timeuntil) s to paint an emote again."
};

local function load_img(path)
	local xsize = ffi.new("int[1]");
	local ysize = ffi.new("int[1]");
	local buf = ffi.gc(stbi.stbi_load(path, xsize, ysize, nil, 4), stbi.stbi_image_free);

	if (buf == nil) then
		origerr = ffi.string(stbi.stbi_failure_reason());
		error("stbimage: "..origerr);
	end

	return {buf=buf, xsize=xsize[0], ysize=ysize[0]};
end

local function shuf(tbl)
	for i=#tbl,1,-1 do
		local swapi = math.random(i);
		local tmp = tbl[i];

		tbl[i] = tbl[swapi];
		tbl[swapi] = tmp;
	end
end

local function start(pid, path)
	img[pid] = load_img(path);

	local pos = get_position(pid);
	placeoff[pid] = {x=pos.x-img[pid].xsize/2, y=pos.y-img[pid].ysize/2};

	placepos[pid] = {};
	for y=0,img[pid].ysize-1 do
		for x=0,img[pid].xsize-1 do
			table.insert(placepos[pid], {x, y});
		end
	end

	shuf(placepos[pid]);

	placetimer[pid] = get_time();
end

local cmd = {name="emote", usage="image", desc="Paint an image centered at your location."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv <= 1);

	if (argv[1] == nil) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, use_emotelist_msg);
		return;
	end

	local now = get_time();
	if (now < timeouttimer[pid]) then
		l10n_send_chat(pid, timeout_msg, {timeuntil=math.ceil(timeouttimer[pid]-now)});
		return;
	end

	local image = argv[1];

	if (string.find(image, "[./\\]")) then
		l10n_send_chat(pid, bad_image_msg);
		return;
	end

	origerr = nil;
	local status, err = pcall(start, pid, emote_basedir..image..emote_suffix);

	if (not status) then
		if (origerr == "can't fopen" or origerr == "Unable to open file") then
			l10n_send_chat(pid, bad_image_msg);
			return;
		end
		error(err, 2);
	end

	timeouttimer[pid] = now + emote_timeout;
end
register_command(cmd, mod);

local function patesc(str)
	return string.gsub(str, "[$^()%%.%[%]*+-?]", "%%%0").."$";
end

local cmd = {name="emotelist", fakepid=true, desc="List images available to paint on the ground."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	local suffixpat = patesc(emote_suffix);
	local list = {};
	for file in lfs.dir(emote_basedir) do
		if (string.find(file, "^[^./\\]*"..suffixpat)) then
			local image = string.gsub(file, suffixpat, "", 1);
			table.insert(list, image);
		end
	end

	-- TODO: dedup line-wrapping code with command_cmds.lua
	local line = "";
	local pfx = "";

	table.sort(list);
	for _,image in ipairs(list) do
		line = line..pfx..image;
		pfx = ", ";

		if (#line >= 75) then
			if (i ~= #list) then
				line = line..",";
			end

			server_msg(pid, line);

			line = "";
			pfx = "";
		end
	end

	server_msg(pid, line);
end
register_command(cmd, mod);

local function set_highest_point(pos)
	for z=0,62 do
		pos.z = z;

		if (is_solid(pos)) then
			return;
		end
	end

	pos.z = 63;
end

local function place_next(pid)
	local x, y;
	local r, g, b, a;

	a = 0;
	while (a == 0 and placepos[pid] ~= 0) do
		local pos = table.remove(placepos[pid]);
		x = pos[1];
		y = pos[2];

		local off = (y*img[pid].xsize + x) * 4;
		r = img[pid].buf[off];
		g = img[pid].buf[off+1];
		b = img[pid].buf[off+2];
		a = img[pid].buf[off+3];
	end

	if (a ~= 0) then
		local pos = {x=(placeoff[pid].x+x) % 512, y=(placeoff[pid].y+y) % 512};
		set_highest_point(pos);

		set_block_color(get_anon_pid(), {r=r, g=g, b=b});
		if (emote_sync) then
			block_action(pos, 0, get_anon_pid());
		else
			send_block_action(PID_BROADCAST, pos, 0, get_anon_pid());
		end

		placetimer[pid] = placetimer[pid] + 0.005;
	end

	if (#placepos[pid] == 0) then
		img[pid] = nil;
		placepos[pid] = nil;
		placeoff[pid] = nil;
		placetimer[pid] = nil;
	end
end

function mod.after.tick()
	local now = get_time();

	for i in piditer(PID_BROADCAST) do
		while (placetimer[i] and now >= placetimer[i]) do
			place_next(i);
		end
	end
end

return mod;
