#include "state.h"
#include "demoncore.h"
#include <stdio.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
#define SEND(pid, data) st->f.send_packet(pid, &(data), sizeof(data), st)
#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define IP(pid) host_ip(&st->host->peers[pid].address)
#define PORT(pid) (st->host->peers[pid].address.port)

const char *host_ip(ENetAddress *addr);

static void on_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, struct State *st) {
	st->f.register_grenade(pid, st->p[pid].team, pos, vel, fuse, st);
	st->f.send_grenade(PID_BROADCAST_EXCEPT(pid), pos, vel, fuse, 0, st);
}

static void on_reload(plid pid, unsigned mag, unsigned reserve, struct State *st) {
	/* TODO: use mag, reserve for something */
	st->f.send_reload(PID_BROADCAST_EXCEPT(pid), 255, 255, pid, st);
}

static void on_any_connect(plid pid, struct State *st) {
	if (pid >= MAX_PLAYERS) {
		LOG("%s:%u (#%u) attempted to connect but server was full", IP(pid), PORT(pid), pid);
		/* TODO: should i disconnect_now or just disconnect? if just disconnect, should i increase the amount of connections? */
		enet_peer_disconnect_now(st->host->peers+pid, 4);
	} else
		st->f.on_successful_connect(pid, st);
}

static void on_successful_connect(plid pid, struct State *st) {
	LOG("%s:%u (#%u) connected", IP(pid), PORT(pid), pid);

	st->f.send_map(pid, st);
}

static void on_disconnect(plid pid, struct State *st) {
	LOG("%s:%u (#%u) disconnected", IP(pid), PORT(pid), pid);

	if (st->p[pid].joined) {
		struct PacketPlayerLeft pl;

		pl.packetID = PacketTypePlayerLeft;
		pl.playerID = pid;

		SEND(PID_BROADCAST, pl);
	}

	st->p[pid].joined = 0;
	st->p[pid].alive = 0;

	st->f.after_player_destroy(pid, st);
}

/* TODO: CP437, etc. . . */
static void on_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	plid dest;

	LOG("(%s) %s: %s", type == ChatTypeAll ? "Global" : "Team", st->p[pid].name, msg);

	if (type == ChatTypeAll)
		dest = PID_BROADCAST;
	else
		dest = PID_BROADCAST_TEAM(st->p[pid].team);

	st->f.send_chat(dest, msg, type, pid, st);
}

static void on_join(plid pid, unsigned team, unsigned weapon, const char *name, struct State *st) {
	LOG("%s:%u (#%u) joined as \"%s\"", IP(pid), PORT(pid), pid, name);

	/* at this point the player is still not alive */
	st->p[pid].joined = 1;
	st->p[pid].score = 0;
	st->p[pid].newteam = team;
	st->p[pid].newweapon = weapon;
	strcpy(st->p[pid].name, name);

	st->f.spawn_player(pid, st);
}

static void on_switch(plid pid, unsigned team, unsigned weapon, struct State *st) {
	/* TODO: should its use as :kill be permitted? */
	//LOG("%s:%u (#%u) tried to switch or something", IP(pid), PORT(pid), pid);

	st->p[pid].newteam = team;
	st->p[pid].newweapon = weapon;

	/* TODO: should spectators haven't a respawn timer? */
	/* TODO: how to only switch after spawn? */
	if (st->p[pid].team == 255) {
		st->f.spawn_player(pid, st);
		if (st->p[pid].alive)
			st->f.kill(pid, KillTypeTeamChange, 0, st);
	} else if (team != st->p[pid].team && st->p[pid].alive)
		st->f.kill(pid, KillTypeTeamChange, 0, st);
	else if (st->p[pid].alive)
		st->f.kill(pid, KillTypeWeaponChange, 0, st);
}

static void on_tool_change(plid pid, unsigned tool, struct State *st) {
	struct PacketSetTool set;

	st->p[pid].tool = tool;

	set.packetID = PacketTypeSetTool;
	set.playerID = pid;
	set.tool = tool;

	SEND(PID_BROADCAST_EXCEPT(pid), set);
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

static void on_move_input(plid pid, unsigned bitmask, struct State *st) {
	struct PacketInput in;

	/* TODO: validate uncrouch? handle openspades jump */
	if ((bitmask & KeyStateTypeCrouch) ^ (st->p[pid].inputs & KeyStateTypeCrouch))
		change_crouch(bitmask & KeyStateTypeCrouch, st->p+pid, st->globals.map.solidData, 0);

	if (bitmask & KeyStateTypeJump && st->p[pid].airborne)
		bitmask &= ~KeyStateTypeJump;

	st->p[pid].inputs = bitmask;

	in.packetID = PacketTypeInput;
	in.playerID = pid;
	in.keyStates = bitmask;

	SEND(PID_BROADCAST_EXCEPT(pid), in);
}

static void on_mouse_input(plid pid, unsigned bitmask, struct State *st) {
	struct PacketWeaponInput in;

	st->p[pid].mouseInputs = bitmask;

	in.packetID = PacketTypeWeaponInput;
	in.playerID = pid;
	in.weaponInput = bitmask;

	SEND(PID_BROADCAST_EXCEPT(pid), in);
}

/* TODO: can dead men shoot in openspades if they haven't received a Kill? */
static void on_hit(plid pid, unsigned type, plid hitPlayer, struct State *st) {
	if (st->p[pid].team != st->p[hitPlayer].team)
		st->f.set_hp_directional(hitPlayer, st->p[hitPlayer].hp - st->f.get_hit_damage(pid, type, st), st->p[pid].pos, st);

	if (st->p[hitPlayer].hp == 0)
		st->f.kill(hitPlayer, type == HitTypeMelee ? KillTypeMelee : type == HitTypeHead, pid, st);
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
}
