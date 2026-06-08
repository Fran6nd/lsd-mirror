-- base_logging.lua -- Spit various bits of stuff out to the log
local bit = require("bit");
local mod = init_mod();

local connect_full_log_msg = {
	en="%(addr):%(port) attempted to connect but server was full"
};

local connect_log_msg = {
	en="%(addr):%(port) (#%(pid)) connected"
};

local disconnect_name_log_msg = {
	en="%(addr):%(port) (#%(pid)) (%(name)) disconnected"
};

local disconnect_log_msg = {
	en="%(addr):%(port) (#%(pid)) disconnected"
};

local join_log_msg = {
	en="%(addr):%(port) (#%(pid)) joined as \"%(name)\""
};

local version_log_msg = {
	en="%(addr):%(port) (#%(pid)) got version: '%(char)' (%(charval)) v%(major).%(minor).%(patch): %(msg)"
};

local version_ext_log_msg = {
	en="%(addr):%(port) (#%(pid)) got version-ext: \"%(cli)\" v%(major).%(minor).%(patch), %(flags), %(lang)"
};

-- TODO: stick ipaddr in here or just go off pid?
local global_chat_log_msg = {
	en="(Global) %(name) (#%(pid)): %(msg)"
};

local team_chat_log_msg = {
	en="(Team) %(name) (#%(pid)): %(msg)"
};

-- TODO: move to core?
local function get_ipaddr_str(pid)
	addr = get_ipaddr(pid);
	return string.format("%u.%u.%u.%u",
		bit.band(bit.rshift(addr, 24), 0xff),
		bit.band(bit.rshift(addr, 16), 0xff),
		bit.band(bit.rshift(addr,  8), 0xff),
		bit.band(addr, 0xff)
	);
end

local function logtbl(pid, tbl)
	if (tbl == nil) then
		tbl = {};
	end

	tbl.pid = pid;
	tbl.addr = get_ipaddr_str(pid);
	tbl.port = get_udp_port(pid);

	return tbl;
end

function mod.before.on_any_connect(pid)
	if (pid >= MAX_PLAYERS) then
		l10n_log(connect_full_log_msg, logtbl(pid));
	end
end

function mod.before.on_successful_connect(pid)
	l10n_log(connect_log_msg, logtbl(pid));
end

function mod.before.on_disconnect(pid)
	if (is_joined(pid)) then
		-- TODO: function to get if player was ever joined
		l10n_log(disconnect_name_log_msg, logtbl(pid, {name=get_name(pid)}));
		return;
	end

	l10n_log(disconnect_log_msg, logtbl(pid));
end

function mod.before.on_join(pid, team, gun, name)
	l10n_log(join_log_msg, logtbl(pid, {name=name}));
end

function mod.before.on_version(pid, idChar, major, minor, patch, msg)
	l10n_log(version_log_msg, logtbl(pid, {
		char=idChar < 0x20 or idChar >= 0x7f and '?' or string.char(idChar),
		charval=idChar,
		major=major,
		minor=minor,
		patch=patch,
		msg=msg
	}));
end

function mod.before.on_version_ext(pid, major, minor, patch, flags, cli, lang)
	l10n_log(version_ext_log_msg, logtbl(pid, {
		cli=cli,
		major=major,
		minor=minor,
		patch=patch,
		flags=string.format("0x%04x", flags),
		lang=lang
	}));
end

-- TODO: Or should I hook player_msg()? I.e., should I log messages from muted players or not?
function mod.before.on_chat(pid, msg, type)
	l10n_log(type == 0 and global_chat_log_msg or team_chat_log_msg, {
		name=get_name(pid),
		pid=pid,
		msg=msg
	});
end

--[[ TODO:
src/funcs_packetrecv.c:125:	LOG("Z: %f", PACKET.pos.z);
src/funcs_packetrecv.c:136:	if (PACKET.pos.z - st->p[pid].lastagreedpos.z > DOWNWARD_SPEED_LIMIT) LOG("dist1: %f", PACKET.pos.z - st->p[pid].lastagreedpos.z);
src/funcs_packetrecv.c:139:	if (sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos) > HORIZONTAL_SPEED_LIMIT_SQR) LOG("dist2: %f", sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos));
src/funcs_packetrecv.c:141:	if (sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos) > COMBINED_SPEED_LIMIT_SQR) LOG("dist3: %f", sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos));
src/funcs_packetrecv.c:295:	if (sqr_dist2(st->p[pid].lastagreedpos, st->p[PACKET.playerID].pos) > 128*128+HORIZONTAL_SPEED_LIMIT_SQR) LOG("dist2: %f", sqr_dist2(st->p[pid].lastagreedpos, st->p[PACKET.playerID].pos));
src/funcs_packetrecv.c:595:	LOG("%s:%u (#%u) sent crap packet, ID %i, name %s, len %lu, __LINE__: %i\r\n\t%s", IP(pid), PORT(pid), pid, length > 0 ? ((uint8_t *)data)[0] : -1, st->crappacketname, (unsigned long)length, st->crapline, st->crapcond);
src/funcs_send.c:170:		LOG1("Some deflate err!");
src/lua.c:523:			LOG("get_spawn_position: %s", luaL_checkstring(l, -1));
src/main.c:332:			LOG("realloc doesn't want to shrink the grenades buffer (%"PRIuSIZET" currently allocated)", st->globals.grenadeSize);
src/main.c:720:			LOG("Out of memory for more grenades (%"PRIuSIZET" currently allocated)", st->globals.grenadeSize);
src/main.c:1079:		LOG("pvx_vxl_stream_stateless: %s", err);
src/main.c:1479:		LOG("%s:%"PRIu16" says HELLO", host_ip(&host->receivedAddress), host->receivedAddress.port);
src/main.c:1490:		LOG("%s:%"PRIu16" says HELLOLAN", host_ip(&host->receivedAddress), host->receivedAddress.port);
]]--

return mod;
