#include "state.h"
#include "demoncore.h"
#include <stdio.h>

clk get_time(void);
clk from_s_double(double ts);

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
#define SEND(pid, data) st->f.send_packet(pid, &(data), sizeof(data), st)
#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define IP(pid) host_ip(&st->host->peers[pid].address)
#define PORT(pid) (st->host->peers[pid].address.port)

const char *host_ip(ENetAddress *addr);

static void on_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	st->f.register_grenade(pid, st->p[pid].team, pos, vel, from_s_double(fuse), st);
	st->f.send_grenade(PID_BROADCAST_EXCEPT(pid), pos, vel, fuse, 0, st);
}

extern const clk reloadTime[3];
extern const clk fireTime[3];

static void on_reload(plid pid, struct State *st) {
	st->f.send_reload(PID_BROADCAST_EXCEPT(pid), 255, 255, pid, st);
	st->p[pid].reloadtime = get_time() + reloadTime[st->p[pid].gun];

	/* Would've been nice if the packet reported what the
	 * client thinks its ammo is. . .
	 *
	 * Guess I should put that on my TODO list -- I could
	 * just set estMagAmmo rather easily after validating.
	 */
}

static void on_any_connect(plid pid, struct State *st) {
	if (pid >= MAX_PLAYERS) {
		LOG("%s:%"PRIu16" (#%"PRIiPID") attempted to connect but server was full", IP(pid), PORT(pid), pid);
		/* TODO: should i disconnect_now or just disconnect? if just disconnect, should i increase the amount of connections? */
		enet_peer_disconnect_now(st->host->peers+pid, 4);
	} else
		st->f.on_successful_connect(pid, st);
}

static void on_successful_connect(plid pid, struct State *st) {
	LOG("%s:%"PRIu16" (#%"PRIiPID") connected", IP(pid), PORT(pid), pid);

	st->f.send_map(pid, st);
}

static void on_disconnect(plid pid, struct State *st) {
	int wasalive = st->p[pid].alive;
	LOG("%s:%"PRIu16" (#%"PRIiPID") disconnected", IP(pid), PORT(pid), pid);

	if (st->p[pid].joined)
		st->f.send_disconnect(PID_BROADCAST, pid, st);

	st->p[pid].joined = 0;
	st->p[pid].alive = 0;

	st->p[pid].bugMask = 0;
	st->p[pid].extMask = 0;
	st->p[pid].initStateSent = 0;
	st->p[pid].wantFingerprint = 0;
	st->p[pid].handshaked = 0;
	st->p[pid].hasverext = 0;
	st->p[pid].idChar = 0;
	st->p[pid].verMajor = 0;
	st->p[pid].verMinor = 0;
	st->p[pid].verPatch = 0;
	st->p[pid].verMsg[0] = '\0';
	st->p[pid].verExtMajor = 0;
	st->p[pid].verExtMinor = 0;
	st->p[pid].verExtPatch = 0;
	st->p[pid].verExtFlags = 0;
	st->p[pid].verExtCliName[0] = '\0';
	st->p[pid].verExtLang[0] = '\0';

	if (wasalive)
		st->f.after_player_destroy(pid, st);
}

/* TODO: CP437, etc. . . */
static void on_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	LOG("(%s) %s: %s", type == ChatTypeAll ? "Global" : "Team", st->p[pid].name, msg);

	st->f.player_msg(msg, type, pid, st);
}

static void on_join(plid pid, unsigned team, unsigned gun, const char *name, struct State *st) {
	LOG("%s:%"PRIu16" (#%"PRIiPID") joined as \"%s\"", IP(pid), PORT(pid), pid, name);

	st->p[pid].score = 0;
	st->p[pid].newteam = team;
	st->p[pid].newgun = gun;
	st->p[pid].wantFingerprint = 0;
	strcpy(st->p[pid].name, name);

	st->f.spawn_player(pid, st->f.get_spawn_position(pid, st), st);
}

static void on_switch(plid pid, unsigned team, unsigned gun, struct State *st) {
	/* TODO: should its use as :kill be permitted? */
	st->p[pid].newteam = team;
	st->p[pid].newgun = gun;

	/* TODO: should spectators haven't a respawn timer? */
	/* TODO: how to only switch after spawn? */
	if (st->p[pid].team == 255) {
		if (!(st->p[pid].bugMask & BS_BUG_NOSHORTPLAYER) || team != 255) {
			st->f.spawn_player(pid, st->f.get_spawn_position(pid, st), st);
			if (st->p[pid].alive)
				st->f.kill(pid, KillTypeTeamChange, 0, st);
		}
	} else if (team != st->p[pid].team && st->p[pid].alive)
		st->f.kill(pid, KillTypeTeamChange, 0, st);
	else if (st->p[pid].alive)
		st->f.kill(pid, KillTypeGunChange, 0, st);
}

static void on_tool_change(plid pid, unsigned tool, struct State *st) {
	st->p[pid].tool = tool;
	st->p[pid].reloadtime = 0;

	st->f.send_set_tool(PID_BROADCAST_EXCEPT(pid), tool, pid, st);

	if (st->p[pid].estfiretime == 0 && st->p[pid].tool == ToolTypeGun && st->p[pid].mouseInputs & 1) {
		st->p[pid].estfiretime = get_time() + fireTime[st->p[pid].gun];
		/* TODO: does this actually need to be here? */
		st->p[pid].reloadtime = 0;

		st->f.before_estimated_fire(pid, st);
		if (st->p[pid].estMagAmmo != 0)
			st->p[pid].estMagAmmo--;
	}
}

/* Win or timeout or advance or something else. */
/* TODO: wonder if it should be given a reason arg */
/* TODO: do you think lua could add extra args to funcs to pass to other lua scripts? */
static void on_game_end(struct State *st) {
	(void)st;
	return;
}

static void on_shutdown(struct State *st) {
	(void)st;
	return;
}

static void on_position(plid pid, fvec3 pos, struct State *st) {
	st->p[pid].pos = pos;
	st->p[pid].lastagreedpos = pos;
}

static void on_orientation(plid pid, fvec3 ori, struct State *st) {
	st->p[pid].ori = ori;
}

/* TODO: that estfiretime-canceling copy/pasted block? */
static void on_move_input(plid pid, unsigned bitmask, struct State *st) {
	/* TODO: validate uncrouch? handle openspades jump */
	if ((bitmask & KeyStateTypeCrouch) ^ (st->p[pid].inputs & KeyStateTypeCrouch))
		change_crouch(bitmask & KeyStateTypeCrouch, st->p+pid, st->globals.map.solidData, 0);

	if (bitmask & KeyStateTypeJump && st->p[pid].airborne)
		bitmask &= ~KeyStateTypeJump;

	st->p[pid].inputs = bitmask;

	st->f.send_move_input(PID_BROADCAST_EXCEPT(pid), bitmask, pid, st);
}

static void on_mouse_input(plid pid, unsigned bitmask, struct State *st) {
	st->p[pid].mouseInputs = bitmask;

	/* TODO: sidestep buggerspades */
	st->f.send_mouse_input(PID_BROADCAST_EXCEPT(pid), bitmask, pid, st);

	if (st->p[pid].estfiretime == 0 && st->p[pid].tool == ToolTypeGun && st->p[pid].mouseInputs & 1) {
		st->p[pid].estfiretime = get_time() + fireTime[st->p[pid].gun];
		st->p[pid].reloadtime = 0;

		st->f.before_estimated_fire(pid, st);
		if (st->p[pid].estMagAmmo != 0)
			st->p[pid].estMagAmmo--;
	}
}

/* TODO: can dead men shoot in openspades if they haven't received a Kill? */
static void on_hit(plid pid, unsigned type, plid hitPlayer, struct State *st) {
	if (st->p[pid].team != st->p[hitPlayer].team) {
		st->f.damage_player_directional(
			hitPlayer,
			st->f.get_hit_damage(pid, type, st),
			st->p[pid].pos,
			type == HitTypeMelee ? KillTypeMelee : type == HitTypeHead,
			pid,
			st
		);
	}
}

/* TODO: sync player's own block color by making abuse of pid 32? that would be very cursed though */
static void on_color_change(plid pid, color color, struct State *st) {
	st->p[pid].blockColor[0] = color[0];
	st->p[pid].blockColor[1] = color[1];
	st->p[pid].blockColor[2] = color[2];

	st->f.send_set_block_color(PID_BROADCAST_EXCEPT(pid), color, pid, st);
}

static void on_block_action(plid pid, ivec3 pos, unsigned type, struct State *st) {
	st->f.block_action(pos, type, pid, st);
}

static void on_block_line(plid pid, ivec3 start, ivec3 end, struct State *st) {
	st->f.block_line(start, end, pid, st);
}

static void on_handshake(plid pid, struct State *st) {
	st->p[pid].handshaked = 1;
	st->p[pid].wantFingerprint = 2;
}

static void on_version(plid pid, unsigned idChar, unsigned major, unsigned minor, unsigned patch, const char *msg, size_t msglen, struct State *st) {
	st->p[pid].wantFingerprint = 1;
	st->p[pid].idChar = idChar;
	st->p[pid].verMajor = major;
	st->p[pid].verMinor = minor;
	st->p[pid].verPatch = patch;

	memcpy(st->p[pid].verMsg, msg, msglen);
	st->p[pid].verMsg[msglen] = '\0';

	switch (st->p[pid].idChar) {
	case 'B':
		/* Tigerspades usually identifies as >=0.1.6, though was 0.1.5 when UTF-8 was introduced */
		st->p[pid].bugMask |= BS_BUG_INFLOOR | BS_BUG_NOSHORTPLAYER | BS_BUG_SCREWED_DISCONNECT_DATA | QUIRK_OS_CP437 |
		(major >= 0 && minor >= 1 && patch >= 6 ? QUIRK_UTF8 : QUIRK_ASCII);
		break;
	case 'o':
		/* TODO: do i want to unset QUIRK_UTF8 if not set in version-ext? */
		st->p[pid].bugMask |= QUIRK_UTF8 | QUIRK_OS_CP437;

		if (strstr(st->p[pid].verMsg, "ZeroSpades") || strstr(st->p[pid].verMsg, "IV of Spades"))
			st->p[pid].bugMask |= BS_BUG_SCREWED_DISCONNECT_DATA;
		break;
	}

	/* TODO: still have to sanitize/reencode strings */
	LOG("%s:%"PRIu16" (#%"PRIiPID") got version: '%c' (%"PRIu8") v%"PRIu8".%"PRIu8".%"PRIu8": %s", IP(pid), PORT(pid), pid, idChar < 0x20 || idChar >= 0x7f ? '?' : idChar, idChar, major, minor, patch, st->p[pid].verMsg);
}

static void on_version_ext(plid pid, unsigned major, unsigned minor, unsigned patch, uint32_t flags, const char *cli, size_t clilen, const char *lang, size_t langlen, struct State *st) {
	st->p[pid].hasverext = 1;
	st->p[pid].wantFingerprint = 0;
	st->p[pid].verExtMajor = major;
	st->p[pid].verExtMinor = minor;
	st->p[pid].verExtPatch = patch;
	st->p[pid].verExtFlags = flags;

	memcpy(st->p[pid].verExtCliName, cli, clilen);
	st->p[pid].verExtCliName[clilen] = '\0';

	memcpy(st->p[pid].verExtLang, lang, langlen);
	st->p[pid].verExtLang[langlen] = '\0';

	/* TODO: *still* still have to sanitize/reencode strings */
	LOG("%s:%"PRIu16" (#%"PRIiPID") got version-ext: \"%s\" v%"PRIu8".%"PRIu8".%"PRIu8", %#06"PRIx32", %s", IP(pid), PORT(pid), pid, st->p[pid].verExtCliName, major, minor, patch, flags, st->p[pid].verExtLang);
}

void set_funcs_event(struct State *st) {
	st->f.on_any_connect = on_any_connect;
	st->f.on_successful_connect = on_successful_connect;
	st->f.on_disconnect = on_disconnect;
	st->f.on_join = on_join;
	st->f.on_switch = on_switch;
	st->f.on_chat = on_chat;
	st->f.on_tool_change = on_tool_change;
	st->f.on_block_action = on_block_action;
	st->f.on_position = on_position;
	st->f.on_orientation = on_orientation;
	st->f.on_move_input = on_move_input;
	st->f.on_mouse_input = on_mouse_input;
	st->f.on_color_change = on_color_change;
	st->f.on_block_line = on_block_line;
	st->f.on_hit = on_hit;
	st->f.on_grenade = on_grenade;
	st->f.on_reload = on_reload;
	st->f.on_game_end = on_game_end;
	st->f.on_shutdown = on_shutdown;
	st->f.on_handshake = on_handshake;
	st->f.on_version = on_version;
	st->f.on_version_ext = on_version_ext;
}
