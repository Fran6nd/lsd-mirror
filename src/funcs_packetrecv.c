#include "state.h"
#include "demoncore.h"
#include <stdio.h>
#include <math.h>

#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define IP(pid) host_ip(&st->host->peers[pid].address)
#define PORT(pid) (st->host->peers[pid].address.port)
const char *host_ip(ENetAddress *addr);

#define CAT2(x,y) x##y
#define CAT(x,y) CAT2(x,y)

#define STR2(x) #x
#define STR(x) STR2(x)

#define BADRETURN do {st->crapline = __LINE__; return 1;} while (0)
#define SBAD(cond) do {if (cond) {st->crapcond = "SBAD("#cond");"; BADRETURN;}} while (0)
#define SCASEANY case CAT(PacketType, PCKT): st->crappacketname = STR(PCKT);
#define SCASEJOINED SCASEANY SBAD(!st->p[pid].joined);
#define SCASEALIVE SCASEANY SBAD(!st->p[pid].alive);
#define PCASE case CAT(PacketType, PCKT):
#define PACKET (*(struct CAT(Packet, PCKT) *)packet->data)
#define PACKETPTR ((struct CAT(Packet, PCKT) *)packet->data)
#define SRANGE(min, max) SBAD(packet->dataLength < (min) || packet->dataLength > (max))
#define SEXACT() SBAD(packet->dataLength != sizeof(struct CAT(Packet, PCKT)))
#define SNUL() SBAD(packet->data[packet->dataLength-1] != '\0')
#define SPID() SBAD(packet->data[1] != pid)

#define SCLIP(xoff, yoff, zoff, vec) clip_player(vec.x + (xoff), vec.y + (yoff), vec.z + (zoff), st->globals.map.solidData, 0)
#define SCLIPB(zoff, vec) (SCLIP(-0.44, -0.44, zoff, vec) || SCLIP (-0.44, 0.44, zoff, vec) || SCLIP(0.44, -0.44, zoff, vec) || SCLIP(0.44, 0.44, zoff, vec))
static int stuck_in_a_block(fvec3 pos, struct State *st) {
		return SCLIPB(1.34, pos) || SCLIPB(0.45, pos) || SCLIPB(-0.44, pos);
}

/* TODO: sometimes voxlap and rl trigger this on ori with <0.000001 */
#define CLOSE_ENOUGH_TO_1(x) (fabsf((x) - 1) < 0.00005)

#define HORIZONTAL_SPEED_LIMIT 10.4
#define DOWNWARD_SPEED_LIMIT 32.5403 /* Normally just 32, but sometimes OpenSpades likes to send more */
#define UPWARD_SPEED_LIMIT 11.52 /* TODO: why was this 13.52 before */ /* TODO: needs some tweaking */
#define COMBINED_SPEED_LIMIT 32.16

/* TODO: does spawning affect openspades' position send time? didn't i already mention this somewhere? */
/* TODO: horizontal speed limit inexplicably being screwed at 94-ish min with openspades (most seen: 115.501671) */
#define HORIZONTAL_SPEED_LIMIT_SQR 115.6 /* Nominally 108.16 */
#define COMBINED_SPEED_LIMIT_SQR 1069.46 /* Usually 1034.2656, except when it's not */

/* TODO: these ones don't account for positiondata timing fuckery -- document that outside of this todo! */
#define PLAYER_VEL_LIMIT 1.005
#define PLAYER_VEL_LIMIT_SQR 1.010025
#define PLAYER_HVEL_LIMIT_SQR 0.1056250
#define PLAYER_DVEL_LIMIT_SQR 1
#define PLAYER_UVEL_LIMIT_SQR 0.1296

#define NADE_VEL_LIMIT_SQR 4.020025
#define NADE_HVEL_LIMIT_SQR 1.755625
#define NADE_DVEL_LIMIT_SQR 4
#define NADE_UVEL_LIMIT_SQR 1.8496

#define NOT_THE_SAME_POSITION(p1, p2) (p1.x != p2.x || p1.y != p2.y || p1.z != p2.z)

extern const uint8_t initialMagAmmo[3];
extern const uint8_t initialReserveAmmo[3];

static float sqr_len2(fvec3 vec) {
	return vec.x*vec.x + vec.y*vec.y;
}

static float sqr_dist2(fvec3 pos1, fvec3 pos2) {
	pos1.x -= pos2.x;
	pos1.y -= pos2.y;

	return sqr_len2(pos1);
}

static float sqr_len3(fvec3 vec) {
	return vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
}

static float sqr_dist3(fvec3 pos1, fvec3 pos2) {
	pos1.x -= pos2.x;
	pos1.y -= pos2.y;
	pos1.z -= pos2.z;

	return sqr_len3(pos1);
}

static int get_solid3(int32_t x, int32_t y, int32_t z, struct State *st) {
	return pvx_voxel_get_solidity4(st->globals.map.solidData, CALC_I(x, y), z);
}

extern int get_solid(ivec3 pos, struct State *st);

static int neighboring_voxels(ivec3 pos, struct State *st) {
	int32_t off;
	int count = 0;

	for (off=-1;off<2;off+=2)
		count += (uint32_t)(pos.x+off) < 512 && get_solid3(pos.x+off, pos.y, pos.z, st);

	for (off=-1;off<2;off+=2)
		count += (uint32_t)(pos.y+off) < 512 && get_solid3(pos.x, pos.y+off, pos.z, st);

	for (off=-1;off<2;off+=2)
		count += (uint32_t)(pos.z+off) < 64 && get_solid3(pos.x, pos.y, pos.z+off, st);

	return count;
}

static int on_any_packet(plid pid, ENetPacket *packet, struct State *st) {
	st->crappacketname = "?";

	SBAD(packet->dataLength < 1);

	switch (packet->data[0]) {
#define PCKT PositionData
		SCASEALIVE
		SEXACT();

		/* TODO: remove debugging cruft? or embrace it? */
		if (PACKET.pos.z > 62.65)
			LOG("Z: %f", PACKET.pos.z);

		/* TODO: player can't be higher than a certain height without server intervention */
		/* TODO: where did this magic 62.65 number come from? */
		SBAD(PACKET.pos.x < 0.45 || PACKET.pos.x > 511.55);
		SBAD(PACKET.pos.y < 0.45 || PACKET.pos.y > 511.55);
		SBAD(PACKET.pos.z > 62.65);
		/* TODO: make it suck less */
		/* TODO: should it be last agreed or regular flavor? */
		/* TODO: was_ever_not_in_a_block_since_lastagreedpos heuristic? */
		SBAD(stuck_in_a_block(st->p[pid].lastagreedpos, st) && stuck_in_a_block(st->p[pid].pos, st) && stuck_in_a_block(PACKET.pos, st) && NOT_THE_SAME_POSITION(PACKET.pos, st->p[pid].pos));
		if (PACKET.pos.z - st->p[pid].lastagreedpos.z > DOWNWARD_SPEED_LIMIT) LOG("dist1: %f", PACKET.pos.z - st->p[pid].lastagreedpos.z);
		SBAD(PACKET.pos.z - st->p[pid].lastagreedpos.z > DOWNWARD_SPEED_LIMIT);
		SBAD(st->p[pid].lastagreedpos.z - PACKET.pos.z > UPWARD_SPEED_LIMIT);
		if (sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos) > HORIZONTAL_SPEED_LIMIT_SQR) LOG("dist2: %f", sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos));
		SBAD(sqr_dist2(st->p[pid].lastagreedpos, PACKET.pos) > HORIZONTAL_SPEED_LIMIT_SQR);
		if (sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos) > COMBINED_SPEED_LIMIT_SQR) LOG("dist3: %f", sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos));
		SBAD(sqr_dist3(st->p[pid].lastagreedpos, PACKET.pos) > COMBINED_SPEED_LIMIT_SQR);

		return 0;
#undef PCKT
#define PCKT OrientationData
		SCASEALIVE
		SEXACT();

		SBAD(!CLOSE_ENOUGH_TO_1(PACKET.ori.x*PACKET.ori.x + PACKET.ori.y*PACKET.ori.y + PACKET.ori.z*PACKET.ori.z));

		return 0;
#undef PCKT
#define PCKT SetColor
		SCASEALIVE
		SEXACT();
		SPID();

		return 0;
#undef PCKT
#define PCKT Input
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: keyStates -> keys */
		return 0;
#undef PCKT
#define PCKT WeaponInput
		SCASEALIVE
		SEXACT();
		SPID();

		return 0;
#undef PCKT
#define PCKT ChatMessage
		SCASEJOINED
		SRANGE(4, 4+255);
		SPID();
		SNUL();

		SBAD(PACKET.type > 1);

		return 0;
#undef PCKT
#define PCKT ExistingPlayer
		SCASEANY
		SRANGE(13, 28);
		/* OpenSpades doesn't bother with SPID(); */
		SNUL();

		SBAD(st->p[pid].joined && st->p[pid].team != 255);

		/* TODO: should a spectator be allowed to switch to spectator? this doesn't match shortplayer (RENAME: something better; SpectatorSwitch?) either */
		SBAD(PACKET.team > 1 && PACKET.team != 255);
		SBAD(PACKET.weapon > 2);
		SBAD(PACKET.tool != ToolTypeGun);
		/* OpenSpades puts its score (or some other data; I didn't check) in PACKET.score for some reason despite being ignored */
		/* blue, green and red are ignored */

		return 0;
#undef PCKT
#define PCKT ShortPlayerData
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(st->p[pid].team != 255);

		SBAD(PACKET.team > 1);
		SBAD(PACKET.weapon > 2);

		return 0;
#undef PCKT
#define PCKT ChangeTeam
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(!(st->p[pid].bugMask & BS_BUG_NOSHORTPLAYER) && st->p[pid].team == 255);

		SBAD(PACKET.team > 1 && PACKET.team != 255);

		return 0;
#undef PCKT
#define PCKT ChangeWeapon
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(!(st->p[pid].bugMask & BS_BUG_NOSHORTPLAYER) && st->p[pid].team == 255);

		SBAD(PACKET.weapon > 2);

		return 0;
#undef PCKT
#define PCKT Hit
		SCASEALIVE
		SEXACT();

		/* Only decrease maxMagAmmo if client reports not using a spade */
		if (PACKET.type != 4) {
			SBAD(st->p[pid].maxMagAmmo == 0);
			st->p[pid].maxMagAmmo--;
		}

		/* TODO: rename playerID here, it misleads -- playerID is not the player's ID, just the ID of the hit player */
		SBAD(PACKET.playerID > MAX_PLAYERS);
		SBAD(PACKET.playerID == pid);
		/* Remember that spectators are not alive. */
		SBAD(!st->p[PACKET.playerID].alive);

		/* TODO: didn't betterspades suck at this */
		SBAD(!(st->p[pid].mouseInputs & 1));

		SBAD(st->p[pid].tool != ToolTypeGun && st->p[pid].tool != ToolTypeSpade);

		SBAD(st->p[pid].tool == ToolTypeGun && PACKET.type > 3);
		SBAD(st->p[pid].tool == ToolTypeSpade && PACKET.type != 4);

		/* witchcraft-based range validation -- could probably be triggered with enough lag unless the target is stationary */
		if (sqr_dist2(st->p[pid].lastagreedpos, st->p[PACKET.playerID].pos) > 128*128+HORIZONTAL_SPEED_LIMIT_SQR) LOG("dist2: %f", sqr_dist2(st->p[pid].lastagreedpos, st->p[PACKET.playerID].pos));
		SBAD(sqr_dist2(st->p[pid].lastagreedpos, st->p[PACKET.playerID].pos) > 128*128+HORIZONTAL_SPEED_LIMIT_SQR);

		/* TODO: validate spade dist */
		
		return 0;
#undef PCKT
#define PCKT Grenade
		SCASEJOINED /* Dead men can throw nades (unless you're pyspades). */ /* TODO: only allow 1 deadnade */
		SEXACT();
		SPID();

		/* TODO: should this really be here? */
		SBAD(st->p[pid].grenades == 0);
		st->p[pid].grenades--;

		SBAD(st->p[pid].team == 255);

		/* TODO: openspades switches back earlier than it should */
		//SBAD(st->p[pid].tool != ToolTypeGrenade);

		/* TODO: there's some range slightly above 0 and slightly below 3 that is actually used */
		SBAD(PACKET.fuseLength < 0);
		SBAD(PACKET.fuseLength > 3);

		/* TODO: witchcraft position validation */
		SBAD(PACKET.pos.x <= 0 || PACKET.pos.x >= 512);
		SBAD(PACKET.pos.y <= 0 || PACKET.pos.y >= 512);
		SBAD(PACKET.pos.z >= 64);

		/* TODO: should i bother with finding the true up/down values? betterspades ignores them. . . */
		/* TODO: wonder if a fancily-oriented player throws fancily-velocitied nades */
		SBAD(sqr_len3(PACKET.vel) > NADE_VEL_LIMIT_SQR + 1);
		SBAD(sqr_len2(PACKET.vel) > NADE_HVEL_LIMIT_SQR + 1);
		SBAD(PACKET.vel.z > NADE_DVEL_LIMIT_SQR + 1);
		SBAD(-PACKET.vel.z > NADE_UVEL_LIMIT_SQR + 1);

		/* OpenSpades disagrees here */
		/* TODO: didn't betterspades also suck at this */
		//SBAD(!(st->p[pid].mouseInputs & 1));

		return 0;
#undef PCKT
#define PCKT BlockAction
		SCASEALIVE
		SEXACT();
		SPID();

		SBAD((uint32_t)PACKET.pos.x >= 512);
		SBAD((uint32_t)PACKET.pos.y >= 512);
		SBAD((uint32_t)PACKET.pos.z >= 62);

		/* TODO: voxlap block decrement/increment would go here -- just add a callback? */

		switch(st->p[pid].tool) {
		case ToolTypeSpade:
			SBAD(PACKET.type != 1 && PACKET.type != 2);
			break;
		case ToolTypeBlock:
			SBAD(PACKET.type != 0);
			break;
		case ToolTypeGun:
			SBAD(PACKET.type != 1);
			SBAD(st->p[pid].maxMagAmmo == 0);
			st->p[pid].maxMagAmmo--;
			break;
		default:
			st->crapcond = "BlockAction with invalid tool (probably grenade)";
			BADRETURN;
		}

		/* TODO: needs hard-crap and soft-crap packets */
		switch (PACKET.type) {
		case 0: /* build */
			SBAD(get_solid(PACKET.pos, st));
			SBAD(neighboring_voxels(PACKET.pos, st) == 0);
			SBAD(st->p[pid].blocks == 0);
			break;
		case 1: /* destroy, destroy 3x */
		case 2:
			SBAD(!get_solid(PACKET.pos, st));
			//SBAD(neighboring_voxels(PACKET.pos, st) == 6);
			break;
		}

		return 0;
#undef PCKT
#define PCKT BlockLine
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: limit action/line/hit to farthest hypothetical position given last agreed position */

		SBAD((uint32_t)PACKET.start.x >= 512);
		SBAD((uint32_t)PACKET.start.y >= 512);
		SBAD((uint32_t)PACKET.start.z >= 62);

		SBAD((uint32_t)PACKET.end.x >= 512);
		SBAD((uint32_t)PACKET.end.y >= 512);
		SBAD((uint32_t)PACKET.end.z >= 62);

		/* Length can't be > 50 */
		SBAD(1+abs(PACKET.end.x-PACKET.start.x)+abs(PACKET.end.y-PACKET.start.y)+abs(PACKET.end.z-PACKET.start.z) > 50);

		SBAD(st->p[pid].tool != ToolTypeBlock);

		/* TODO: block lines still work when the start is solid, right? */
		SBAD(neighboring_voxels(PACKET.start, st) == 0);

		SBAD(get_solid(PACKET.end, st));
		SBAD(neighboring_voxels(PACKET.end, st) == 0);

		return 0;
#undef PCKT
#define PCKT SetTool
		SCASEALIVE
		SEXACT();
		SPID();

		/* TODO: ammo, etc. */
		SBAD(PACKET.tool > 3);

		return 0;
#undef PCKT
#define PCKT WeaponReload
		SCASEALIVE
		SEXACT();
		SPID();

		SBAD(st->p[pid].tool != ToolTypeGun);
		SBAD(st->p[pid].mouseInputs);

		/* Sometimes voxlap likes to send 255,255 for ammo and other
		 * times it likes to send 0,0 -- don't depend on it, anyway. . .
		 */

		return 0;
#undef PCKT
#define PCKT HandshakeReturn
		SCASEANY
		SEXACT();

		SBAD(!st->p[pid].wantFingerprint);
		SBAD(st->p[pid].handshaked);
		SBAD(PACKET.challenge != 0xdeadbeef);

		return 0;
#undef PCKT
#define PCKT VersionResponse
		SCASEANY
		SRANGE(5, 5+256);
		/* Not NUL terminated for some reason (I think anyway) --
		 * That's why I decided to use +256 instead of +255 for the size
		 */

		/* TODO: dig into the datagrams if you feel like denying fingerprint sooner than join */
		SBAD(!st->p[pid].wantFingerprint);
		SBAD(PACKET.client == 0);
		SBAD(packet->data[packet->dataLength-1] == '\0');

		return 0;
#undef PCKT
	}

	st->crapcond = "Unknown packet ID";
	BADRETURN;
}

/* TODO: nuke the useless Data from everything, maybe rename WorldUpdate, un-action Kill, annihilate the british, *weapon* reload, . . . */
static void on_sane_packet(plid pid, ENetPacket *packet, struct State *st) {
	switch (packet->data[0]) {
#undef PCKT
#define PCKT PositionData
		PCASE
		st->f.on_position(pid, PACKET.pos, st);
		break;
#undef PCKT
#define PCKT OrientationData
		PCASE
		st->f.on_orientation(pid, PACKET.ori, st);
		break;
#undef PCKT
#define PCKT SetColor
		PCASE
		st->f.on_color_change(pid, PACKET.color, st);
		break;
#undef PCKT
#define PCKT Input
		PCASE
		st->f.on_move_input(pid, PACKET.keyStates, st);
		break;
#undef PCKT
#define PCKT WeaponInput
		PCASE
		st->f.on_mouse_input(pid, PACKET.weaponInput & 3, st);
		break;
#undef PCKT
#define PCKT ChatMessage
		PCASE
		st->f.on_chat(pid, PACKET.message, PACKET.type, st);

		break;
#undef PCKT
#define PCKT ExistingPlayer
		PCASE
		if (st->p[pid].joined)
			st->f.on_switch(pid, PACKET.team, PACKET.weapon, st);
		else
			/* TODO: CP437/WIN-1252 */
			st->f.on_join(pid, PACKET.team, PACKET.team == 255 ? 0 : PACKET.weapon, PACKET.name, st);

		break;
#undef PCKT
#define PCKT ShortPlayerData
		PCASE
		st->f.on_switch(pid, PACKET.team, PACKET.weapon, st);
		break;
#undef PCKT
#define PCKT ChangeTeam
		PCASE
		st->f.on_switch(pid, PACKET.team, st->p[pid].newweapon, st);
		break;
#undef PCKT
#define PCKT ChangeWeapon
		PCASE
		st->f.on_switch(pid, st->p[pid].newteam, PACKET.weapon, st);
		break;
#undef PCKT
#define PCKT Hit
		PCASE
		st->f.on_hit(pid, PACKET.type, PACKET.playerID, st);
		break;
#undef PCKT
#define PCKT Grenade
		PCASE
		/* TODO: blocks and grenades validation */
		st->f.on_grenade(pid, PACKET.pos, PACKET.vel, PACKET.fuseLength, st);
		break;
#undef PCKT
#define PCKT BlockAction
		PCASE
		st->f.on_block_action(pid, PACKET.pos, PACKET.type, st);
		break;
#undef PCKT
#define PCKT BlockLine
		PCASE
		st->f.on_block_line(pid, PACKET.start, PACKET.end, st);
		break;
#undef PCKT
#define PCKT SetTool
		PCASE
		st->f.on_tool_change(pid, PACKET.tool, st);
		break;
#undef PCKT
#define PCKT WeaponReload
		PCASE
		st->f.on_reload(pid, st);
		break;
#undef PCKT
#define PCKT HandshakeReturn
		PCASE
		st->f.on_handshake(pid, st);
		break;
#undef PCKT
#define PCKT VersionResponse
		PCASE
		/* TODO: string */
		st->f.on_version(pid, PACKET.client, PACKET.versionMajor, PACKET.versionMinor, PACKET.versionRevision, st);
		break;
	}
}

static void on_crap_packet(plid pid, ENetPacket *packet, struct State *st) {
	//LOG("%s:%u (#%u) sent crap packet, ID %i, name %s, len %lu, __LINE__: %i\n\t%s", IP(pid), PORT(pid), pid, packet->dataLength > 0 ? packet->data[0] : -1, st->crappacketname, (unsigned long)packet->dataLength, st->crapline, st->crapcond);
	/* TODO: remove need for \r with linenoise */
	LOG("%s:%u (#%u) sent crap packet, ID %i, name %s, len %lu, __LINE__: %i\r\n\t%s", IP(pid), PORT(pid), pid, packet->dataLength > 0 ? packet->data[0] : -1, st->crappacketname, (unsigned long)packet->dataLength, st->crapline, st->crapcond);

	if (packet->dataLength > 0)
	switch (packet->data[0]) {
	case PacketTypePositionData:
		/* OLD TODO: or should it just be a kick -- set to pos or lastagreedpos? */
		/* NEW TODO: probably not a kick considering this has a chance of being validly triggered (in blocks) */
		/* TODO: what if i set_position a dead guy? what if i'd like to spawn where i die? */
		/* TODO: does sending position screw with client's position timing? */
		/* TODO: would it be worth just limiting the magnitude? */
		/* TODO: do i need a function that's just like set_position except used for position resend context? */
		if (st->p[pid].alive) {
			st->p[pid].lastagreedpos = st->p[pid].pos;
			st->f.send_position(pid, st->p[pid].pos, st);
		}
		break;
	}
}

void set_funcs_packetrecv(struct State *st) {
	st->f.on_any_packet = on_any_packet;
	st->f.on_sane_packet = on_sane_packet;
	st->f.on_crap_packet = on_crap_packet;
}
