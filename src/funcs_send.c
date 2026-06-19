#include "state.h"
#include "budgetvxl.h"
#include <isa-l.h>
#include <stdio.h>

clk get_time(void);
clk to_s(clk ts);

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
#define SEND(pid, data) st->f.send_packet(pid, &(data), sizeof(data), st)
#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define LOG1(x) do {st->f.before_log(st); fputs(x"\n", stderr); st->f.after_log(st);} while (0)

int pid_matches(plid broadcast, plid pid, struct State *st);
static int send_packet_flags(plid pid, const void *data, size_t length, unsigned flags, struct State *st) {
	ENetPacket *packet;

	packet = enet_packet_create(data, length, flags);
	if (packet == NULL)
		return -1;

	if (pid == PID_BROADCAST)
		enet_host_broadcast(st->host, 0, packet);
	else if ((uint32_t)pid < MAX_PLAYERS)
		return st->p[pid].peer == NULL ? 0 : (enet_peer_send(st->p[pid].peer, 0, packet) == 0 ? 0 : -1);
	else {
		plid i;
		for (i=0;i<MAX_PLAYERS;i++) {
			if (pid_matches(pid, i, st) && st->p[i].peer != NULL)
				enet_peer_send(st->p[i].peer, 0, packet);
		}
	}

	return 0;
}

static int send_packet(plid pid, const void *data, size_t length, struct State *st) {
	return send_packet_flags(pid, data, length, ENET_PACKET_FLAG_RELIABLE, st);
}

static int send_packet_unreliable(plid pid, const void *data, size_t length, struct State *st) {
	return send_packet_flags(pid, data, length, 0, st);
}

static void send_fog(plid pid, color color, struct State *st) {
	struct PacketFogColor cf;

	cf.packetID = PacketTypeFogColor;
	cf.a = 0;
	cf.color[0] = color[0];
	cf.color[1] = color[1];
	cf.color[2] = color[2];

	SEND(pid, cf);
}

static void send_player_update(plid pid, struct State *st) {
	struct PacketWorldUpdate upd;
	plid i, max = -1;

	upd.packetID = PacketTypeWorldUpdate;

	memset(upd.players, 0, sizeof(upd.players));

	for (i=0;i<MAX_PLAYERS;i++) {
		/* TODO: do i care about the position of dead people? spectators? */
		if (!st->p[i].alive)
			continue;

		max = i;

		upd.players[i].pos = st->p[i].pos;
		upd.players[i].ori = st->p[i].ori;
	}

	st->f.send_packet_unreliable(pid, &upd, 1+(max+1)*24, st);
}

static void send_grenade(plid pid, fvec3 pos, fvec3 vel, float fuse, plid from, struct State *st) {
	struct PacketGrenade nade;

	nade.packetID = PacketTypeGrenade;
	nade.playerID = from;
	nade.fuseLength = fuse;
	nade.pos = pos;
	nade.vel = vel;

	SEND(pid, nade);
}

static void send_reload(plid pid, unsigned mag, unsigned reserve, plid from, struct State *st) {
	struct PacketGunReload rl;

	/* TODO: might need to limit to 254 instead of 255 */
	rl.packetID = PacketTypeGunReload;
	rl.playerID = from;
	rl.magazineAmmo = mag > 255 ? 255 : mag;
	rl.reserveAmmo = reserve > 255 ? 255 : reserve;

	SEND(pid, rl);
}

static void send_map_start(plid pid, unsigned size, struct State *st) {
	struct PacketMapStart ms;

	ms.packetID = PacketTypeMapStart;
	ms.mapSize = size;

	SEND(pid, ms);
}

/* buf should be cols*65*4 bytes */
static size_t get_vxl_chunk(void *buf, size_t coloff, size_t cols, struct State *st) {
	uint_fast32_t x, y;

	if (cols > 512*512-coloff)
		cols = 512*512-coloff;

	x = coloff % 512;
	y = coloff / 512;

	return pvx_dump_vxl(&st->globals.map, x, y, 512, 512, 64, buf, cols);
}

static void init_deflate(struct isal_zstream *stream) {
	isal_deflate_init(stream);

	stream->flush = NO_FLUSH;
	stream->gzip_flag = IGZIP_ZLIB;
	stream->end_of_stream = 0;
	stream->level = 2;
}

static void send_compressed_map_unpristine(plid pid, struct State *st) {
	struct isal_zstream stream;
	size_t cols;
	/* TODO: determine isa-l magic numbers */
	uint8_t outbuf[1+512*8*65*4+330];
	uint8_t isalbuf[ISAL_DEF_LVL2_DEFAULT];

	/* TODO NOTE: doesn't pyspades make a whole new copy of the map every time it wants to write something? efficiency. */
	outbuf[0] = PacketTypeMapChunk;

	init_deflate(&stream);
	stream.level_buf = isalbuf;
	stream.level_buf_size = sizeof(isalbuf);

#pragma omp parallel for ordered
	for (cols=0;cols<512*512;cols += 512*8) {
		size_t buflen;
		uint8_t vxlbuf[512*8*65*4];
		buflen = get_vxl_chunk(vxlbuf, cols, 512*8, st);

#pragma omp ordered
		{
			stream.next_in = vxlbuf;
			stream.avail_in = buflen;

			if (cols == 512*512-512*8)
				stream.end_of_stream = 1;

			do {
				stream.next_out = outbuf+1;
				stream.avail_out = 512*8*65*4+330;

				if (isal_deflate(&stream) != ISAL_DECOMP_OK) {
					LOG1("Some deflate err!");
					break;
				}

				/* Sometimes isal_deflate doesn't actually return anything immediately. */
				if (stream.next_out != outbuf+1)
					st->f.send_packet(pid, outbuf, stream.next_out-outbuf, st);
			} while (stream.avail_in != 0);
		}
	}
}

static void send_compressed_map(plid pid, struct State *st) {
	size_t i;
	uint8_t outbuf[1+8192];
	outbuf[0] = PacketTypeMapChunk;

	if (st->globals.pristineBuf) {
		for (i=0;i<st->globals.pristineLen;i += 8192) {
			size_t len = 8192;
			if (i+len > st->globals.pristineLen)
				len = st->globals.pristineLen - i;

			memcpy(outbuf+1, st->globals.pristineBuf+i, len);
			st->f.send_packet(pid, outbuf, 1+len, st);
		}

	} else
		send_compressed_map_unpristine(pid, st);
}

static void send_map(plid pid, struct State *st) {
	plid i;

	st->f.send_map_start(pid, 0, st);
	send_compressed_map(pid, st);

	/* TODO: should the iterator be moved to send_state? */
	for (i=0;i<MAX_PLAYERS;i++)
		if (pid_matches(pid, i, st))
			st->f.send_state(i, st);
}

static void fill_in_state(struct PacketStateData *sta, plid from, const char teamname[][10], const color *teamcolor, color fog) {
	sta->packetID = PacketTypeStateData;
	sta->playerID = from;
	sta->fog[0] = fog[0];
	sta->fog[1] = fog[1];
	sta->fog[2] = fog[2];
	sta->teamcolor[0][0] = teamcolor[0][0];
	sta->teamcolor[0][1] = teamcolor[0][1];
	sta->teamcolor[0][2] = teamcolor[0][2];
	sta->teamcolor[1][0] = teamcolor[1][0];
	sta->teamcolor[1][1] = teamcolor[1][1];
	sta->teamcolor[1][2] = teamcolor[1][2];
	/* Each team name is 10 bytes; this memsets both of them. TODO: or just use two separate memsets? */
	memset(sta->team1Name, 0, 20);
	strcpy(sta->team1Name, teamname[0]);
	strcpy(sta->team2Name, teamname[1]);
}

static void send_state_ctf(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, const unsigned *teamscore, unsigned maxscore, const plid *holders, const fvec3 *intelpos, const fvec3 *tentpos, struct State *st) {
	struct PacketStateData sta;
	unsigned i;

	fill_in_state(&sta, from, teamname, teamcolor, fog);
	sta.gamemode = 0;
	sta.gm.ctf.teamscore[0] = teamscore[0];
	sta.gm.ctf.teamscore[1] = teamscore[1];
	sta.gm.ctf.maxscore = maxscore;
	/* TODO: I like how the ordering is reversed from what you'd expect */
	/* TODO: why does openspades sometimes decide nobody is holding an intel */
	sta.gm.ctf.heldIntels = (holders[1] != -1) | ((holders[0] != -1) << 1);
	/* TODO: what happens if 255 holds an intel */
	for (i=0;i<2;i++) {
		if (holders[i] != -1) {
			memset(&sta.gm.ctf.intelloc[i], 0, sizeof(sta.gm.ctf.intelloc[i]));
			sta.gm.ctf.intelloc[i].playerID = holders[i];
		} else
			sta.gm.ctf.intelloc[i].position = intelpos[i];
	}
	sta.gm.ctf.tentpos[0] = tentpos[0];
	sta.gm.ctf.tentpos[1] = tentpos[1];

	st->f.send_packet(pid, &sta, 84, st);
}

static void send_state_tc(plid pid, plid from, const char teamname[][10], const color *teamcolor, color fog, unsigned tentcount, const fvec3 *tentpos, unsigned *tentteam, struct State *st) {
	struct PacketStateData sta;
	unsigned i;

	fill_in_state(&sta, from, teamname, teamcolor, fog);
	sta.gamemode = 1;
	sta.gm.tc.territoryCount = tentcount;

	for (i=0;i<tentcount;i++) {
		sta.gm.tc.territories[i].pos = tentpos[i];
		sta.gm.tc.territories[i].team = tentteam[i];
	}

	/* TODO: we should probably validate tentcount from lua code */
	st->f.send_packet(pid, &sta, 33+tentcount, st);
}

static void send_state(plid pid, struct State *st) {
	st->f.send_connected_players(pid, st);
	if (!st->p[pid].initStateSent)
		st->f.demand_fingerprint(pid, st);
	st->f.send_state_ctf(pid, pid, st->globals.teamname, st->globals.teamcolor, st->globals.fog, st->globals.teamscore, st->globals.maxscore, st->globals.intelplayers, st->globals.intelpos, st->globals.tentpos, st);
	st->p[pid].initStateSent = 1;
}

static void send_existing_player(plid pid, unsigned team, unsigned gun, unsigned tool, unsigned score, color blockColor, const char *name, plid from, struct State *st) {
	struct PacketExistingPlayer ep;

	/* TODO: better validate the name length */
	ep.packetID = PacketTypeExistingPlayer;
	ep.playerID = from;
	ep.team = team;
	ep.gun = gun;
	ep.tool = tool;
	ep.score = score;
	ep.blue = blockColor[0];
	ep.green = blockColor[1];
	ep.red = blockColor[2];
	strcpy(ep.name, name);

	/* TODO: sure hope this doesn't involve lots of copying when sent through lua */
	st->f.send_packet(pid, &ep, 13+strlen(ep.name), st);
}

static void send_spawn_player(plid pid, fvec3 pos, unsigned gun, unsigned team, const char *name, plid from, struct State *st) {
	struct PacketCreatePlayer cr;
	plid i;

	cr.packetID = PacketTypeCreatePlayer;
	cr.playerID = from;
	cr.gun = gun;
	cr.team = team;
	cr.pos = pos;
	/* TODO: check strlen */
	strcpy(cr.name, name);

	for (i=0;i<MAX_PLAYERS;i++) {
		if (pid_matches(pid, i, st)) {
			/* This -2 is here because *sane* clients always subtract 2 from CreatePlayer z.
			 * BetterSpades is not sane, since it was based on piqueserver.
			 * The +0.4 can be blamed on ZeroSpades.
			 */
			if      (st->p[i].bugMask & BS_BUG_INFLOOR)
				cr.pos.z = pos.z - 2;
			else if (st->p[i].bugMask & QUIRK_INSKY)
				cr.pos.z = pos.z + 0.4;
			else
				cr.pos.z = pos.z;

			SEND(i, cr);
		}
	}
}

static void send_connected_players(plid pid, struct State *st) {
	plid i;
	
	for (i=0;i<MAX_PLAYERS;i++) {
		if (!st->p[i].joined)
			continue;

		st->f.send_existing_player(pid, st->p[i].team, st->p[i].gun, st->p[i].tool, st->p[i].score, st->p[i].blockColor, st->p[i].name, i, st);

		if (st->p[i].inputs != 0)
			st->f.send_move_input(pid, st->p[i].inputs, i, st);

		/* TODO: deal with mouse input desync? */
		if (st->p[i].mouseInputs != 0)
			st->f.send_mouse_input(pid, st->p[i].mouseInputs, i, st);

		/* TODO: actually send spawn delta? */
		if (st->p[i].team != 255 && !st->p[i].alive)
			st->f.send_kill(pid, 0, KillTypeFall, 0, i, st);
	}
}

static void send_move_input(plid pid, unsigned inputs, plid from, struct State *st) {
	struct PacketInput in;

	in.packetID = PacketTypeInput;
	in.playerID = from;
	in.keyStates = inputs;

	SEND(pid, in);
}

/* TODO: rename this too? it's slightly more specific than mere mouse input (excluding betterspades) */
static void send_mouse_input(plid pid, unsigned inputs, plid from, struct State *st) {
	struct PacketMouseInput mi;

	mi.packetID = PacketTypeMouseInput;
	mi.playerID = from;
	mi.input = inputs;

	SEND(pid, mi);
}

static void send_kill(plid pid, clk spawndelta, unsigned type, plid killer, plid from, struct State *st) {
	struct PacketKill kl;

	kl.packetID = PacketTypeKill;
	kl.playerID = from;
	kl.killerID = killer;
	kl.killType = type;
	kl.respawnTime = to_s(spawndelta + 500000000);

	SEND(pid, kl);
}

static void send_chat(plid pid, const char *msg, unsigned type, plid from, struct State *st) {
	size_t msglen = strlen(msg);
	uint8_t *chat = malloc(3 + msglen + 1);

	if (chat == NULL)
		ERR("malloc");

	chat[0] = PacketTypeChatMessage;
	chat[1] = from;
	chat[2] = type;
	memcpy(chat+3, msg, msglen+1);

	st->f.send_packet(pid, chat, 3 + msglen + 1, st);

	free(chat);
}

static void send_restock(plid pid, plid from, struct State *st) {
	struct PacketRestock rs;

	rs.packetID = PacketTypeRestock;
	rs.playerID = from;

	SEND(pid, rs);
}

static void send_disconnect(plid pid, plid from, struct State *st) {
	struct PacketPlayerLeft dc;

	dc.packetID = PacketTypePlayerLeft;
	dc.playerID = from;

	SEND(pid, dc);
}

static void send_block_action(plid pid, ivec3 pos, unsigned type, plid from, struct State *st) {
	struct PacketBlockAction ba;

	ba.packetID = PacketTypeBlockAction;
	ba.playerID = from;
	ba.type = type;
	ba.pos = pos;

	SEND(pid, ba);
}

static void send_block_line(plid pid, ivec3 start, ivec3 end, plid from, struct State *st) {
	struct PacketBlockLine bl;

	bl.packetID = PacketTypeBlockLine;
	bl.playerID = from;
	bl.start = start;
	bl.end = end;

	SEND(pid, bl);
}

static void send_set_block_color(plid pid, color color, plid from, struct State *st) {
	struct PacketSetColor sc;

	sc.packetID = PacketTypeSetColor;
	sc.playerID = from;
	sc.color[0] = color[0];
	sc.color[1] = color[1];
	sc.color[2] = color[2];

	SEND(pid, sc);
}

static void send_set_tool(plid pid, unsigned tool, plid from, struct State *st) {
	struct PacketSetTool set;

	set.packetID = PacketTypeSetTool;
	set.playerID = from;
	set.tool = tool;

	SEND(pid, set);
}

static void send_position(plid pid, fvec3 pos, struct State *st) {
	struct PacketPositionData pd;

	pd.packetID = PacketTypePositionData;
	pd.pos = pos;

	SEND(pid, pd);
}

static void send_orientation(plid pid, fvec3 ori, struct State *st) {
	struct PacketPositionData od;

	od.packetID = PacketTypeOrientationData;
	od.pos = ori;

	SEND(pid, od);
}

static void send_intel_capture(plid pid, int winning, plid from, struct State *st) {
	struct PacketIntelCapture ic;

	ic.packetID = PacketTypeIntelCapture;
	ic.playerID = from;
	ic.winning = !!winning;

	SEND(pid, ic);
}

static void send_intel_pickup(plid pid, plid from, struct State *st) {
	struct PacketIntelPickup ip;

	ip.packetID = PacketTypeIntelPickup;
	ip.playerID = from;

	SEND(pid, ip);
}

static void send_intel_drop(plid pid, fvec3 pos, plid from, struct State *st) {
	struct PacketIntelDrop id;

	id.packetID = PacketTypeIntelDrop;
	id.playerID = from;
	id.pos = pos;

	SEND(pid, id);
}

static void send_move_object(plid pid, fvec3 pos, unsigned id, unsigned team, struct State *st) {
	struct PacketMoveObject ob;

	ob.packetID = PacketTypeMoveObject;
	ob.objectID = id;
	ob.team = team;
	ob.pos = pos;

	SEND(pid, ob);
}

void set_funcs_send(struct State *st) {
	st->f.send_packet = send_packet;
	st->f.send_packet_unreliable = send_packet_unreliable;
	st->f.send_map = send_map;
	st->f.send_state = send_state;
	st->f.send_chat = send_chat;
	st->f.send_block_action = send_block_action;
	st->f.send_connected_players = send_connected_players;
	st->f.send_player_update = send_player_update;
	st->f.send_position = send_position;
	st->f.send_block_line = send_block_line;
	st->f.send_set_block_color = send_set_block_color;
	st->f.send_set_tool = send_set_tool;
	st->f.send_reload = send_reload;
	st->f.send_intel_capture = send_intel_capture;
	st->f.send_intel_pickup = send_intel_pickup;
	st->f.send_intel_drop = send_intel_drop;
	st->f.send_state_ctf = send_state_ctf;
	st->f.send_state_tc = send_state_tc;
	st->f.send_restock = send_restock;
	st->f.send_disconnect = send_disconnect;
	st->f.send_move_object = send_move_object;
	st->f.send_map_start = send_map_start;
	st->f.send_grenade = send_grenade;
	st->f.send_fog = send_fog;
	st->f.send_orientation = send_orientation;
	st->f.send_existing_player = send_existing_player;
	st->f.send_move_input = send_move_input;
	st->f.send_mouse_input = send_mouse_input;
	st->f.send_kill = send_kill;
	st->f.send_spawn_player = send_spawn_player;
}
