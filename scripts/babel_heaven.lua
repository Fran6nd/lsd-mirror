-- babel_heaven.lua -- Paint over that big thing in the sky
local mod = init_mod();
local ffi = require("ffi");
local lfs = require("lfs");
local stbi = require("lib_stbimage");

local timeouttimer = 0;

local origerr = nil;
local img = nil;
local placepos = nil;
local placetimer = nil;

-- Also uses babel_width, babel_height, babel_z from babel.lua
getcfg("babel_heaven_default", "heaven/default.png");
getcfg("babel_heaven_basedir", "heaven/");
getcfg("babel_heaven_suffix", ".png");
getcfg("babel_heaven_timeout", 5*60);

local bad_image_msg = {
	en="Cannot find image; try /heavenlist."
};

local use_heavenlist_msg = {
	en="You can list images with /heavenlist."
};

local timeout_msg = {
	en="You must wait %(timeuntil) s to paint over the platform again."
};

local function load_img(path)
	local xsize = ffi.new("int[1]");
	local ysize = ffi.new("int[1]");
	local buf = ffi.gc(stbi.stbi_load(path, xsize, ysize, nil, 3), stbi.stbi_image_free);

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

local function start(path)
	img = load_img(path);

	assert(img.xsize == babel_width, "Image width is not "..babel_width.." px");
	assert(img.ysize == babel_height, "Image height is not "..babel_height.." px");

	placepos = {};

	for y=0,img.ysize-1 do
		for x=0,img.xsize-1 do
			table.insert(placepos, {x, y});
		end
	end

	shuf(placepos);

	placetimer = get_time();
end

local function build_now(path)
	img = load_img(path);

	assert(img.xsize == babel_width, "Image width is not "..babel_width.." px");
	assert(img.ysize == babel_height, "Image height is not "..babel_height.." px");

	local ptr = img.buf;
	for y=0,img.ysize-1 do
		for x=0,img.xsize-1 do
			local r = ptr[0];
			local g = ptr[1];
			local b = ptr[2];
			ptr = ptr + 3;

			set_block_color(get_anon_pid(), {r=r, g=g, b=b});
			block_action({x=256-babel_width/2+x,y=256-babel_height/2+y,z=babel_z}, 0, get_anon_pid());
		end
	end
end

function mod.after.babel_build_platform(mapload, pass2)
	if (pass2) then
		return;
	end

	if (mapload) then
		build_now(babel_heaven_default);
	else
		start(babel_heaven_default);
	end
end

local function do_heaven(pid, cmd, argv, force)
	cmd_assert(pid, cmd, #argv <= 1);

	if (argv[1] == nil) then
		send_usage(pid, cmd);
		l10n_send_chat(pid, use_heavenlist_msg);
		return;
	end

	local now = get_time();
	if (force or now >= timeouttimer) then
		local image = argv[1];

		if (string.find(image, "[./\\]")) then
			l10n_send_chat(pid, bad_image_msg);
			return;
		end

		origerr = nil;
		local status, err = pcall(start, babel_heaven_basedir..image..babel_heaven_suffix);

		if (not status) then
			if (origerr == "can't fopen" or origerr == "Unable to open file") then
				l10n_send_chat(pid, bad_image_msg);
				return;
			end
			error(err, 2);
		end

		timeouttimer = now + babel_heaven_timeout;
	else
		l10n_send_chat(pid, timeout_msg, {timeuntil=math.ceil(timeouttimer-now)});
	end
end

local cmd = {name="heaven", caps="heaven", fakepid=true, usage="image", desc="Paint an image on the babel platform."};
function cmd.func(pid, argv)
	do_heaven(pid, cmd, argv);
end
register_command(cmd, mod);

local cmd = {name="forceheaven", caps="forceheaven", fakepid=true, usage="image", desc="Paint an image on the babel platform regardless of the timer's opinion."};
function cmd.func(pid, argv)
	do_heaven(pid, cmd, argv, true);
end
register_command(cmd, mod);

local function patesc(str)
	return string.gsub(str, "[$^()%%.%[%]*+-?]", "%%%0").."$";
end

local cmd = {name="heavenlist", fakepid=true, desc="List images available to paint on the babel platform."};
function cmd.func(pid, argv)
	cmd_assert(pid, cmd, #argv == 0);

	local suffixpat = patesc(babel_heaven_suffix);
	local list = {};
	for file in lfs.dir(babel_heaven_basedir) do
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

local function place_next()
	local pos = table.remove(placepos);
	local x = pos[1];
	local y = pos[2];
	local off = (y*img.xsize + x) * 3;

	local r = img.buf[off];
	local g = img.buf[off+1];
	local b = img.buf[off+2];

	set_block_color(get_anon_pid(), {r=r, g=g, b=b});
	block_action({x=256-babel_width/2+x,y=256-babel_height/2+y,z=babel_z}, 0, get_anon_pid());

	if (#placepos ~= 0) then
		placetimer = placetimer + 0.005;
	else
		img = nil;
		placepos = nil;
		placetimer = nil;
	end
end

function mod.after.tick()
	while (placetimer and get_time() >= placetimer) do
		place_next();
	end
end

return mod;
