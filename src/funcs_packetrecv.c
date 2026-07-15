#include "state.h"
#include "demoncore.h"
#include <stdio.h>
#include <math.h>

#define LOG(x, ...) do {st->f.before_log(st); fprintf(stderr, x"\n", __VA_ARGS__); st->f.after_log(st);} while (0)
#define IP(pid) (st->p[pid].peer ? host_ip(&st->p[pid].peer->address) : "<local>")
#define PORT(pid) (st->p[pid].peer ? st->p[pid].peer->address.port : 0)
const char *host_ip(ENetAddress *addr);

#define CAT2(x,y) x##y
#define CAT(x,y) CAT2(x,y)

#define STR2(x) #x
#define STR(x) STR2(x)

#define BADRETURN do {st->crapline = __LINE__; st->crapsilence = 0; return 1;} while (0)
#define BADRETURNSILENT do {st->crapline = __LINE__; st->crapsilence = 1; return 1;} while (0)
#define SBAD(cond) do {if (cond) {st->crapcond = "SBAD("#cond");"; BADRETURN;}} while (0)
#define SBADSILENT(cond) do {if (cond) {st->crapcond = "SBAD("#cond");"; BADRETURNSILENT;}} while (0)
#define SCASEANY case CAT(PacketType, PCKT): st->crappacketname = STR(PCKT);
#define SCASEJOINED SCASEANY SBAD(!st->p[pid].joined);
#define SCASEALIVE SCASEANY SBADSILENT(!st->p[pid].alive);
#define PCASE case CAT(PacketType, PCKT):
#define PACKET (*(struct CAT(Packet, PCKT) *)data)
#define PACKETPTR ((struct CAT(Packet, PCKT) *)data)
#define SRANGE(min, max) SBAD(length < (min) || length > (max))
#define SEXACT() SBAD(length != sizeof(struct CAT(Packet, PCKT)))
#define SNUL() SBAD(((uint8_t *)data)[length-1] != '\0')
#define SPID() SBAD(((uint8_t *)data)[1] != pid)

#define SCLIP(xoff, yoff, zoff, vec) clip_phys(vec.x + (xoff), vec.y + (yoff), vec.z + (zoff), st->globals.map.solidData, 0)
#define SCLIPB(zoff, vec) (SCLIP(-0.44, -0.44, zoff, vec) || SCLIP (-0.44, 0.44, zoff, vec) || SCLIP(0.44, -0.44, zoff, vec) || SCLIP(0.44, 0.44, zoff, vec))
static int stuck_in_a_block(fvec3 pos, struct State *st) {
		return SCLIPB(1.34, pos) || SCLIPB(0.45, pos) || SCLIPB(-0.44, pos);
}

/* TODO: sometimes voxlap and rl trigger this on ori with <0.000001 */
#define CLOSE_ENOUGH_TO_1(x) (fabsf((x) - 1) < 0.0001)

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

static int on_any_packet(plid pid, const void *data, size_t length, struct State *st) {
	uint16_t packetID;
	int wantQuirks;

	st->crappacketname = "?";

	SBAD(length < 1);

	if (((uint8_t *)data)[0] & 128) {
		SBAD(length < 2);
		memcpy(&packetID, data, 2);
	} else
		packetID = ((uint8_t *)data)[0];

	/* This must be &'d with 1 on receipt of *any* packet,
	 * but the Quirks packet needs to know the state of
	 * it before it was reset, so we keep a local copy.
	 */
	wantQuirks = st->p[pid].wantQuirks;
	st->p[pid].wantQuirks &= 1;

	switch (packetID) {
#define PCKT PositionData
		SCASEALIVE
		SEXACT();

		/* TODO: remove debugging cruft? or embrace it? */
		if (PACKET.pos.z > 62.65)
			LOG("Z: %f", PACKET.pos.z);

		/* TODO: player can't be higher than a certain height without server intervention */
		/* TODO: where did this magic 62.65 number come from? */
		SBAD(!isfinite(PACKET.pos.x) || PACKET.pos.x < 0.45 || PACKET.pos.x > 511.55);
		SBAD(!isfinite(PACKET.pos.y) || PACKET.pos.y < 0.45 || PACKET.pos.y > 511.55);
		SBAD(!isfinite(PACKET.pos.z) || PACKET.pos.z > 62.65);
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
#define PCKT MouseInput
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
		SBAD(memchr(data+3, length-4, '\0'));

		return 0;
#undef PCKT
#define PCKT ExistingPlayer
		/* This packet ID is used both for ExistingPlayer and some custom OpenSpades version-getting junk */
		SCASEANY
		SRANGE(7, 2 + 2+3+15 + 2+5 + 2+4);

		if (((char *)data)[1] == 'x' && ((uint8_t *)data)[2] == 0 && ((uint8_t *)data)[3] >= 3) {
			/* "Enhanced" version send */
			/* The prior SRANGE applies here */
			SBAD(st->p[pid].wantFingerprint != 1);

			const uint8_t *dat = data;
			uint8_t memblen;

			dat += 2;
			length -= 2;

			#define VEREXT_FIELD(id, badmembsizecond, domemchr, memchroff) \
				SBADSILENT(length < 2); \
				SBAD(dat[0] != (id)); \
				memblen = dat[1]; \
\
				SBAD(memblen > length - 2 || (badmembsizecond)); \
				if (domemchr) SBAD(memchr(data+2+memchroff, memblen-memchroff, '\0')); \
				dat += 2 + memblen; \
				length -= 2 + memblen;

			VEREXT_FIELD(0, memblen < 3 || memblen > 3+15, 1, 3);
			VEREXT_FIELD(1, memblen != 2 && memblen != 5, 1, 0);
			VEREXT_FIELD(2, memblen != 4, 0, 0);
			SBAD(length != 0);
		} else {
			/* ExistingPlayer */
			SRANGE(13, 28);
			/* OpenSpades doesn't bother with SPID(); */
			/* TODO: do i have to memchr this for NULs? */
			SNUL();

			SBAD(st->p[pid].joined && st->p[pid].team != 255);

			/* TODO: should a spectator be allowed to switch to spectator? this doesn't match shortplayer (RENAME: something better; SpectatorSwitch?) either */
			SBAD(PACKET.team > 1 && PACKET.team != 255);
			SBAD(PACKET.gun > 2);
			SBAD(PACKET.tool != ToolTypeGun);
			/* OpenSpades puts its score (or some other data; I didn't check) in PACKET.score for some reason despite being ignored */
			/* blue, green and red are ignored */
		}

		return 0;
#undef PCKT
#define PCKT ShortPlayerData
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(st->p[pid].team != 255);

		SBAD(PACKET.team > 1);
		SBAD(PACKET.gun > 2);

		return 0;
#undef PCKT
#define PCKT ChangeTeam
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(!(st->f.has_quirk(pid, QUIRK_NOSHORTPLAYER, st)) && st->p[pid].team == 255);

		SBAD(PACKET.team > 1 && PACKET.team != 255);

		return 0;
#undef PCKT
#define PCKT ChangeGun
		SCASEJOINED
		SEXACT();
		SPID();

		SBAD(!(st->f.has_quirk(pid, QUIRK_NOSHORTPLAYER, st)) && st->p[pid].team == 255);

		SBAD(PACKET.gun > 2);

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
		SBAD(!isfinite(PACKET.fuseLength));
		SBAD(PACKET.fuseLength < 0);
		SBAD(PACKET.fuseLength > 3);

		/* TODO: witchcraft position validation */
		SBAD(!isfinite(PACKET.pos.x) || PACKET.pos.x <= 0 || PACKET.pos.x >= 512);
		SBAD(!isfinite(PACKET.pos.y) || PACKET.pos.y <= 0 || PACKET.pos.y >= 512);
		SBAD(!isfinite(PACKET.pos.z) || PACKET.pos.z >= 64);

		/* TODO: should i bother with finding the true up/down values? betterspades ignores them. . . */
		/* TODO: wonder if a fancily-oriented player throws fancily-velocitied nades */
		SBAD(!isfinite(PACKET.vel.x) || !isfinite(PACKET.vel.y) || !isfinite(PACKET.vel.z));
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

		/* Length can't be greater than whatever amount of blocks the player has */
		SBAD(1+abs(PACKET.end.x-PACKET.start.x)+abs(PACKET.end.y-PACKET.start.y)+abs(PACKET.end.z-PACKET.start.z) > st->p[pid].blocks);

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
#define PCKT GunReload
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

		SBAD(st->p[pid].wantFingerprint < 3);
		SBAD(st->p[pid].handshaked);
		SBAD(PACKET.challenge != 0xdeadbeef);

		return 0;
#undef PCKT
#define PCKT VersionResponse
		SCASEANY
		/* Not NUL terminated for some reason */
		SRANGE(5, 5+255);

		/* TODO: dig into the datagrams if you feel like denying fingerprint sooner than join */
		SBADSILENT(st->p[pid].wantFingerprint == 1);
		SBAD(st->p[pid].wantFingerprint < 2);
		SBAD(PACKET.client == 0);
		SBAD(memchr(data+5, length-5, '\0'));

		return 0;
#undef PCKT
#define PCKT Quirks
		SCASEANY
		/* There is no invalid size for this packet */

		SBAD(wantQuirks != 3);
		return 0;
#undef PCKT
	}

	st->crapcond = "Unknown packet ID";
	BADRETURN;
}

/* TODO: nuke the useless Data from everything, maybe rename WorldUpdate, un-action Kill, annihilate the british, *gun* reload, . . . */
static void on_sane_packet(plid pid, const void *data, size_t length, struct State *st) {
	switch (((uint8_t *)data)[0]) {
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
#define PCKT MouseInput
		PCASE
		st->f.on_mouse_input(pid, PACKET.input & 3, st);
		break;
#undef PCKT
#define PCKT ChatMessage
		PCASE
		st->f.on_chat(pid, PACKET.message, PACKET.type, st);

		break;
#undef PCKT
#define PCKT ExistingPlayer
		PCASE
		if (((char *)data)[1] == 'x' && ((uint8_t *)data)[2] == 0 && ((uint8_t *)data)[3] >= 3) {
			const uint8_t *dat = data;

			unsigned major, minor, patch;
			uint32_t flags;
			const char *cli, *lang;
			size_t clilen, langlen;

			dat += 3;

			major = dat[1];
			minor = dat[2];
			patch = dat[3];
			cli = (const char *)dat+1+3;
			clilen = dat[0]-3;
			dat += 1 + dat[0] + 1;

			lang = (const char *)dat+1;
			langlen = dat[0];
			dat += 1 + dat[0] + 1;

			memcpy(&flags, dat+1, sizeof(uint32_t));
			st->f.on_version_ext(pid, major, minor, patch, flags, cli, clilen, lang, langlen, st);
		} else {
			if (st->p[pid].joined)
				st->f.on_switch(pid, PACKET.team, PACKET.gun, st);
			else
				/* TODO: CP437/WIN-1252 */
				st->f.on_join(pid, PACKET.team, PACKET.team == 255 ? 0 : PACKET.gun, PACKET.name, st);
		}

		break;
#undef PCKT
#define PCKT ShortPlayerData
		PCASE
		st->f.on_switch(pid, PACKET.team, PACKET.gun, st);
		break;
#undef PCKT
#define PCKT ChangeTeam
		PCASE
		st->f.on_switch(pid, PACKET.team, st->p[pid].newgun, st);
		break;
#undef PCKT
#define PCKT ChangeGun
		PCASE
		st->f.on_switch(pid, st->p[pid].newteam, PACKET.gun, st);
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
#define PCKT GunReload
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
		st->f.on_version(pid, PACKET.client, PACKET.versionMajor, PACKET.versionMinor, PACKET.versionRevision, PACKET.operatingSystemInfo, length-5, st);
		break;
#undef PCKT
#define PCKT Quirks
		PCASE
		st->f.on_quirks(pid, ((char *)data)+1, length-1, st);
		break;
	}
}

static void on_crap_packet(plid pid, const void *data, size_t length, struct State *st) {
	int32_t packetID;

	if (length > 1 && ((uint8_t *)data)[0] & 128)
		memcpy(&packetID, data, 2);
	else if (length > 0 && !(((uint8_t *)data)[0] & 128))
		packetID = ((uint8_t *)data)[0];
	else
		packetID = -1;

	if (!st->crapsilence) LOG(
		"%s:%u (#%u) sent crap packet, ID %i, name %s, len %lu, __LINE__: %i\n\t%s",
		IP(pid),
		PORT(pid),
		pid,
		packetID,
		st->crappacketname,
		(unsigned long)length,
		st->crapline,
		st->crapcond
	);

	if (length > 0)
	switch (((uint8_t *)data)[0]) {
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
