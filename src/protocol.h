/** @file protocol.h
 * Contains enums and structures essential to the AoS protocol.
 */

#ifndef LIBSPADES_PROTOCOL_H
#define LIBSPADES_PROTOCOL_H
#ifdef __cplusplus
extern "C" {
#endif

#define PACK_GCC

#include <stdint.h>

#define VARIABLE_LENGTH 1
/* TODO: 256 */
#define MAX_PLAYERS 32

#ifdef PACK_GCC
#define LIBSPADES_PACKED __attribute__((packed))
#else
#pragma pack(push, 1)
#define LIBSPADES_PACKED
#endif

typedef struct LIBSPADES_PACKED {float x, y, z;} fvec3p;
typedef struct LIBSPADES_PACKED {int32_t x, y, z;} ivec3p;
typedef uint8_t color[3];

/* core protocol packet structs */

/** Changes the position of the client player @ingroup packets
 * @note NaN values will cause the player to get immediately kicked on most
 * servers.
 *
 * @piquebug NaN values will also bypass votekicks, and allow the player to
 * rejoin with no repurcussions.
 *
 * @piquebug Trusted players on piqueserver have rubberbanding disabled,
 * which allows them to teleport anywhere on the map! (Even Z level
 * 18446744073709551616!)
 *
 * @piquebug Player positions are not limited to the map's dimensions.
 * I suggest a position limit of at least something like {0, 0, 63} to {512,
 * 512, -8}
 *
 * @piquebug Players can no-clip into blocks by simply teleporting into them.
 */
struct LIBSPADES_PACKED PacketPositionData {
	uint8_t packetID; /**< 0 `(Client<->Server)` */
	fvec3p pos;
};

/** Changes the orientation of the client player. @todo document @ingroup packets
 * @note NaN values will cause the player to get immediately kicked on most
 * servers.
 *
 * @piquebug NaN values will also bypass votekicks, and allow the player to
 * rejoin with no repurcussions.
 *
 * @piquebug On piqueserver commits before
 * [41b9af4a9a71cee4b1ca9f342e0862397434ca89](https://github.com/piqueserver/piqueserver/commit/41b9af4a9a71cee4b1ca9f342e0862397434ca89),
 * suddenly changing orientation to {0, 0, -1} while moving may cause the X
 * and Y axes of the player's position to become NaN, without kicking the
 * player. This may cause a hardban if you do something wrong (it is unknown
 * what this wrong thing may be). This even works without trusted! Trusted
 * might help, but further testing is needed to say for certain.
 *
 * @spadesxbug SpadesX copied and pasted the same code used for piqueserver,
 * resulting in the NaN bug being copied with it.
 *
 * @osbug A continuation of the previously mentioned bug, if the player jumps
 * and changes orientation (possibly to {0, 1, -1}) it will immediately crash
 * most connected OpenSpades 0.1.3 clients with an OpenAL error (assuming
 * they have OpenAL enabled). This has also been shown to work on OpenSpades
 * 0.1.5, but it does not seem to work without trusted.
 *
 * @todo maybe refine the steps down a little?
 *
 * @piquebug Just kidding, that bug wasn't actually patched, piqueserver is
 * truly unfixable. My guess is some sort of overflow in to the negatives
 * could be happening. How did I do the bug? I just multiplied my orientation
 * a lot lmao. Let's try FLT_MAX for orientation!
 */
struct LIBSPADES_PACKED PacketOrientationData {
	uint8_t packetID;   /**< 1 `(Client<->Server)*` @details *Client
	                                       compatibility varies, Client-->Server is the only
	                                                                       direction guaranteed to work. */
	fvec3p ori;
};

struct LIBSPADES_PACKED PlayerPositionData {
	fvec3p pos;
	fvec3p ori;
};

struct LIBSPADES_PACKED PlayerPositionData76 {
	uint8_t playerID;
	fvec3p pos;
	fvec3p ori;
};

/** Regularly sent packet containing positions and orientations of all
 * players @ingroup packets
 * @piquebug Involved in a
 * [patched](https://github.com/piqueserver/piqueserver/commit/41b9af4a9a71cee4b1ca9f342e0862397434ca89)
 * bug described on the \ref PacketOrientationData page.
 */
struct LIBSPADES_PACKED PacketWorldUpdate {             /* do 0.76 version later */
	uint8_t packetID;                               /**< 2 `(Client<--Server)` */
	struct PlayerPositionData players[MAX_PLAYERS]; /**< Contains the actual data */
};

/** Changes the movement inputs of a player @ingroup packets
 * @piquebug Involved in a
 * [patched](https://github.com/piqueserver/piqueserver/commit/41b9af4a9a71cee4b1ca9f342e0862397434ca89)
 * bug described on the \ref PacketOrientationData page.
 */
struct LIBSPADES_PACKED PacketInput {
	uint8_t packetID; /**< 3 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player whose movement is changing */
	uint8_t keyStates;
	/**< What movement is taking place.
	 * Possible values (ORed together) are:
	 * - KeyStateTypeUp = 1,
	 * - KeyStateTypeDown = 2,
	 * - KeyStateTypeLeft = 4,
	 * - KeyStateTypeRight = 8,
	 * - KeyStateTypeJump = 16,
	 * - KeyStateTypeCrouch = 32,
	 * - KeyStateTypeSneak = 64,
	 * - KeyStateTypeSprint = 128
	 */
};

/** Changes the weapon inputs of a player @ingroup packets */
struct LIBSPADES_PACKED PacketWeaponInput {
	uint8_t packetID; /**< 4 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player whose weapon input is changing */
	uint8_t weaponInput;
	/**< The weapon input state. Possible values (ORed together) are:
	 * - WeaponInputTypePrimary = 1,
	 * - WeaponInputTypeSecondary = 2
	 */
};

/** Tells the server that a player has been hit @ingroup packets */
struct LIBSPADES_PACKED PacketHit {
	uint8_t packetID; /**< 5 `(Client-->Server)` */
	uint8_t playerID; /**< The ID of the player who has been hit */
	uint8_t type;
	/**< Where/how the player has been hit. Possible values are:
	 * - HitTypeTorso = 0,
	 * - HitTypeHead = 1,
	 * - HitTypeArms = 2,
	 * - HitTypeLegs = 3,
	 * - HitTypeMelee = 4
	 */
};

/** Changes the client player's HP @ingroup packets */
struct LIBSPADES_PACKED PacketSetHP {
	uint8_t packetID; /**< 5 `(Client<--Server)` */
	uint8_t hp;       /**< The amount of HP to change the client player to */
	uint8_t type;
	/**< The reason for the damage. Possible values are:
	 * - HurtTypeFall = 0,
	 * - HurtTypeWeapon = 1
	 */
	fvec3p pos;
};

/** Creates a grenade somewhere in the world @ingroup packets
 * @piquebug Piqueserver removes grenades of players who have left,
 * but most clients do not remove these grenades from their local world,
 * causing them to appear as "duds" when they explode on the client-side.
 * This behaviour is not present in SpadesX.
 *
 * @todo test classic
 *
 * @piquebug Trusted players can teleport grenades wherever they please,
 * making it an even more deadly weapon than killaura on a shotgun/SMG,
 * and an even more destructive tool than a hacked SMG.
 *
 * @piquebug Grenades do not count towards griefing team-kill.
 *
 * @todo find a way to bypass pique #708, or just barely meet requirements,
 * or maybe chuck it in a stupid direction. perhaps i can turn it into a
 * heat-seeking missile?
 */
struct LIBSPADES_PACKED PacketGrenade {
	uint8_t packetID; /**< 6 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player who threw the grenade */
	float fuseLength;
	/**< The time in seconds from grenade creation until it detonates.
	 * @piquebug Piqueserver commits before
	 * [13b5c322e3c557c8a4b24c61020e43d5d6463642](https://github.com/piqueserver/piqueserver/commit/13b5c322e3c557c8a4b24c61020e43d5d6463642)
	 * allow any time to be used, despite the fact that normally the maximum
	 * should be 3.
	 */
	fvec3p pos;
	fvec3p vel;
};

/** Changes the held tool of a player @ingroup packets */
struct LIBSPADES_PACKED PacketSetTool {
	uint8_t packetID; /**< 7 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player whose held tool has changed */
	uint8_t tool;
	/**< The held tool. Possible values are:
	 * - ToolTypeSpade = 0,
	 * - ToolTypeBlock = 1,
	 * - ToolTypeGun = 2,
	 * - ToolTypeGrenade = 3
	 */
};

/** Changes the block color of a player @ingroup packets
 * @todo sRGB or linear RGB?
 */
struct LIBSPADES_PACKED PacketSetColor {
	uint8_t packetID; /**< 8 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player whose block color has changed */
	color color;
};

/** Informs the client of in-game players and the server of the joining
 * client player. @ingroup packets
 * @warning Sending a malformed Existing Player packet may cause piqueserver
 * to permanently ban the player's IP address. This is referred to as a
 * "hardban".
 * @piquebug Piqueserver can leak the player's IP address with its "ban
 * subscribe" feature, potentially adding further insult to injury.
 */
struct LIBSPADES_PACKED PacketExistingPlayer {
	uint8_t packetID; /**< 9 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player */
	uint8_t team;     /**< The team of the player. This may be 0 for the first team,
	                                         1 for the second, and -1 for spectator. */
	uint8_t weapon;
	/**< The weapon of the player. Possible values are:
	 * - WeaponTypeRifle = 0,
	 * - WeaponTypeSMG = 1,
	 * - WeaponTypeShotgun = 2
	 */
	uint8_t tool;
	/**< The held tool of the player. Possible values are:
	 * - ToolTypeSpade = 0,
	 * - ToolTypeBlock = 1,
	 * - ToolTypeGun = 2,
	 * - ToolTypeGrenade = 3
	 */
	uint32_t score;
	/**< The amount of points the player has */ /* or points? */
	uint8_t blue;                               /**< The blue colour value of the player's block */
	uint8_t green;                              /**< The green colour value of the player's block */
	uint8_t red;                                /**< The red colour value of the player's block */
	char name[16];
	/**< The name of the player. Normally the maximum size is 16 bytes, the
	 * last of which is required by piqueserver to be NULL
	 * @todo verify
	 * @piquebug This parameter can be left out entirely without being
	 * hardbanned by piqueserver and causes an OpenSpades bug in @ref
	 * PacketCreatePlayer
	 * @piquebug This parameter is limited incorrectly by piqueserver not to 15 bytes and a NULL terminator,
	 * but rather 15 unicode code points, allowing four times the amount of bytes to be crammed into the player name!
	 * @spadesxbug SpadesX commits before
	 * [0493af85e126ab66e056ff667b1884b328c28f8e](https://github.com/SpadesX/SpadesX/commit/0493af85e126ab66e056ff667b1884b328c28f8e)
	 * allowed player names 16 bytes and one NULL terminator long, instead of 15 bytes and one NULL terminator.
	 * @betterspadesbug BetterSpades commits before
	 * [fc7ab028345306391081953ec9383580b835f391](fc7ab028345306391081953ec9383580b835f391) did not properly
	 * handle player names which were too long, potentially causing crashes or funkiness after the actual name.
	 * @todo verify that betterspades actually fixed it properly, and SpadesX too.
	 */
};

/** Switches the client player's team/weapon from spectator mode @ingroup packets
 * @note This packet is exclusively used by the classic client, not even
 * BetterSpades uses it.
 * @note As for OpenSpades, it instead uses @ref PacketExistingPlayer
 * "Existing Player" for all team/weapon switching.
 * @todo VERIFY
 *
 * @serverbug Sending this packet as an alternative to @ref
 * PacketExistingPlayer "Existing Player" for game join results in a
 * gibberish player name
 */
struct LIBSPADES_PACKED PacketShortPlayerData {
	uint8_t packetID; /**< 10 `(Client-->Server)` */
	uint8_t playerID;
	/**< The ID of the player who changed weapon/team. This parameter should
	 * technically not be necessary since it is never meant to be received by
	 * any clients (instead they should receive @ref PacketCreatePlayer "Create
	 * Player"), but it is still here nonetheless.
	 */
	uint8_t team;   /**< The team to switch the player to */
	uint8_t weapon; /**< The weapon to switch the player to */
};

/** Moves objects. @todo implement and document @ingroup packets */
struct LIBSPADES_PACKED PacketMoveObject {
	uint8_t packetID; /**< 11 `(Client<--Server)` */
	uint8_t objectID;
	uint8_t team; /* unsigned int is intentional for some reason, check the
	                                 aosprotocol docs */
	fvec3p pos;
};

/** Spawns and changes properties of a player @ingroup packets */
struct LIBSPADES_PACKED PacketCreatePlayer {
	uint8_t packetID; /**< 12 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player to spawn/modify */
	uint8_t weapon;   /**< The player's weapon */
	uint8_t team;     /**< The player's team */
	fvec3p pos;
	char name[16];
	/**< The name of the player.
	 * @osbug Abuse of a piqueserver bug in @ref PacketExistingPlayer leads to
	 * the player's name appearing as that of the last disconnected player if
	 * the OpenSpades client connected before the abusing client. */
	/* or connected?? TODO: verify */
};

/** Creates or destroys blocks @ingroup packets */
struct LIBSPADES_PACKED PacketBlockAction {
	uint8_t packetID; /**< 13 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player who created/destroyed the
	                                         block(s) */
	uint8_t type;
	/**< The action to be performed on the block. Possible values are:
	 * - BlockActionTypeBuild = 0,
	 * - BlockActionTypeSpadeGunDestroy = 1,
	 * - BlockActionTypeSpadeSecondaryDestroy = 2,
	 * - BlockActionTypeGrenadeDestroy = 3
	 *
	 * @piquebug Any tool can use any action, however
	 * BlockActionTypeGrenadeDestroy (spadenade) was fixed after commit
	 * [c3cc0963d9c7a16b424214ba02a5cab0cb0d7e1a](https://github.com/piqueserver/piqueserver/commit/c3cc0963d9c7a16b424214ba02a5cab0cb0d7e1a)
	 *
	 * @piquebug In versions that have the BlockActionTypeGrenadeDestroy bug,
	 * removed blocks are client-side only
	 *
	 * @todo verify
	 *
	 * @piquebug BlockActionTypeBuild does not check if a block already exists
	 * at the specified position, making it possible to "paint" blocks that
	 * already exist.
	 */
	ivec3p pos;
};

/** Makes a line of blocks @ingroup packets
 * @todo document
 *
 * @piquebug In commits before
 * [0204ac0a1936af8a10b0f6af8b1896b8356a7fda](https://github.com/piqueserver/piqueserver/commit/0204ac0a1936af8a10b0f6af8b1896b8356a7fda),
 * block lines can still be made if either the start, the end or both do not
 * touch a surface. If the start point does not touch a surface, then the
 * blocks will become "ghost blocks". These blocks only exist client-side and
 * cannot be removed by anything other than a smart client, a reconnect, the
 * spade's alt-dig, grenades, or the "toppling" feature. The end point can
 * also not touch a surface, but this has no effect on whether or not the
 * blocks become "ghost blocks".
 *
 * @piquebug Commits before
 * [0204ac0a1936af8a10b0f6af8b1896b8356a7fda](https://github.com/piqueserver/piqueserver/commit/0204ac0a1936af8a10b0f6af8b1896b8356a7fda)
 * have no or broken rapid-hack detection, making it possible to immediately
 * build entire structures around the player with a size of 12x12x12. When
 * combined with trusted teleportation and the above "ghost block" bug, a
 * hacker can make nearly indestructible prisons around any player!
 */
struct LIBSPADES_PACKED PacketBlockLine {
	uint8_t packetID;        /**< 14 `(Client<->Server)` */
	uint8_t playerID;        /**< The ID of the player who made the block line */
	ivec3p start;
	ivec3p end;
};

union IntelLocation {
	uint8_t playerID;      /**< The ID of the player carrying the intel if any */
	fvec3p position; /**< The position of the intel if no player is carrying it */
};

/** State data for the Capture-the-Flag gamemode. */
struct LIBSPADES_PACKED CTFStateData {
	uint8_t team1Score;   /**< The score (captures) of the first team. */
	uint8_t team2Score;   /**< The score (captures) of the second team. */
	uint8_t captureLimit; /**< The score that needs to be reached to finish the
	                                                 game. */
	uint8_t heldIntels;
	/**< Which teams have a player holding their intel. Possible values (ORed
	 * together) are:
	 * - First team: 1,
	 * - Second team: 2
	 */
	union IntelLocation team1Intel; /**< The location of the first team's intel. Use value
	                                                                   playerID if the team's bit in heldIntels is
	                                   set, otherwise position */
	union IntelLocation team2Intel; /**< The location of the second team's intel. Use value
	                                                                   playerID if the team's bit in heldIntels is
	                                   set, otherwise position */
	fvec3p team1TentPosition; /**< The position of the first team's tent. */
	fvec3p team2TentPosition; /**< The position of the second team's tent. */
};

struct LIBSPADES_PACKED Territory {
	fvec3p pos;
	uint8_t team;    /**< The team of the territory @todo document and implement */
};

/** State data for the Territorial Control gamemode. */
struct LIBSPADES_PACKED TCStateData {
	uint8_t territoryCount; /**< The quantity of territories in the game */
	struct Territory territories[16];
	/**< The territories
	 * @todo remove limit and fix the statedata in gamestate
	 */
};

/** A union that contains the possible gamemodes for @ref PacketStateData */
union GamemodeData {
	struct CTFStateData ctfStateData; /**< Capture-the-Flag mode */
	struct TCStateData tcStateData;   /**< Territorial Control mode */
};

/** Contains information about the game's state and the client player's ID. @ingroup packets
 * Receiving this packet signifies that the map download has ended.
 */
struct LIBSPADES_PACKED PacketStateData { /* TODO: shoehorn gamemode state data in later */
	uint8_t packetID;                 /**< 15 `(Client<--Server)` */
	uint8_t playerID;                 /**< The ID of the client player. */
	uint8_t fog_b;                    /**< The blue colour value of the fog. */
	uint8_t fog_g;                    /**< The green colour value of the fog. */
	uint8_t fog_r;                    /**< The red colour value of the fog. */
	uint8_t team1_b;                  /**< The blue colour value of the first team. */
	uint8_t team1_g;                  /**< The green colour value of the first team. */
	uint8_t team1_r;                  /**< The red colour value of the first team. */
	uint8_t team2_b;                  /**< The blue colour value of the second team. */
	uint8_t team2_g;                  /**< The green colour value of the second team. */
	uint8_t team2_r;                  /**< The red colour value of the second team. */
	char team1Name[10];               /**< The name of the first team. */
	char team2Name[10];               /**< The name of the second team. */
	uint8_t gamemode;
	/**< The gamemode. Possible values are:
	 * - GamemodeTypeCTF = 0,
	 * - GamemodeTypeTC = 1
	 */
	union GamemodeData gamemodeData; /**< The data for the gamemode's state. */
};

/** Informs the client about the death of a player. @ingroup packets */
struct LIBSPADES_PACKED PacketKill {
	uint8_t packetID; /**< 16 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player who died. */
	uint8_t killerID; /**< The ID of the killer. */
	uint8_t killType;
	/**< How the player died. Possible values are:
	 * - KillTypeWeapon = 0,
	 * - KillTypeHeadshot = 1,
	 * - KillTypeMelee = 2,
	 * - KillTypeGrenade = 3,
	 * - KillTypeFall = 4,
	 * - KillTypeTeamChange = 5,
	 * - KillTypeWeaponChange = 6
	 *
	 * @classicegg Values not in the range of 0 to 6 will display as "Derpy
	 * Kill Message".
	 * @todo verify
	 */
	uint8_t respawnTime;
	/**< The time in seconds until the player respawns.
	 * @note Respawning is handled server-side and this should only be used for
	 * countdown timers and the like.
	 */
};

/** Sends a message in chat. @ingroup packets */
struct LIBSPADES_PACKED PacketChatMessage {
	uint8_t packetID; /**< 17 `(Client<->Server)` */
	uint8_t playerID; /**< The ID of the player who sent the message. */
	uint8_t type;
	/**< The type of message. Possible values are:
	 * - ChatTypeAll = 0,
	 * - ChatTypeTeam = 1,
	 * - ChatTypeSystem = 2
	 */
	char message[VARIABLE_LENGTH]; /**< The contents of the chat message. Variable length.
	                                        @todo make fixed-width? */
};

/** A seemingly useless packet to signify the start of the map download. @ingroup packets
 * See also: @ref PacketStateData
 */
struct LIBSPADES_PACKED PacketMapStart {
	uint8_t packetID; /**< 18 `(Client<--Server)` */
	uint32_t mapSize; /**< The size of the zlib stream in bytes. @piquebug This garbage server always sends 1.5 MiB while misidentifying it as 2MB,
	                       even though the stream tends to not be 1.5/2 MiB but in fact a varying length. */
};

/** A slightly more useful v0.76 equivalent of @ref PacketMapStart @ingroup packets */
struct LIBSPADES_PACKED PacketMapStart76 {
	uint8_t packetID;                              /**< 18 `(Client<--Server)` */
	uint32_t mapSize;                              /**< The compressed map size in bytes. */
	uint32_t crc32;                                /**< Presumably the CRC32 code for the decompressed map.
	                                                                      @todo verify and implement */
	char mapName[16]; /**< The name of the map. */ /* may not actually work */
};

/** Contains a chunk of a zlib stream of map data. @ingroup packets
 * The map chunks seem to be compressed with zlib and DEFLATE.
 */
struct LIBSPADES_PACKED PacketMapChunk {
	uint8_t packetID;                 /**< 19 `(Client<--Server)` */
	uint8_t mapData[VARIABLE_LENGTH]; /**< The chunk of zlib-compressed map data. Variable length.
	                                           Usually the maximum size received is 8192 B */
};

/** Informs the client that a player has left the game. @ingroup packets */
struct LIBSPADES_PACKED PacketPlayerLeft {
	uint8_t packetID; /**< 20 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player who left. */
};

/** Informs the client that a territory has been captured. @ingroup packets */
struct LIBSPADES_PACKED PacketTerritoryCapture {
	uint8_t packetID; /**< 21 `(Client<--Server)` */
	uint8_t territoryID; /**< Corresponds to territory index in StateData. */
	uint8_t winning; /**< Whether the capture was the last needed to end the game. */
	uint8_t team; /**< The team which captured the territory. */
};

/** @ingroup packets
 * @todo document and implement
 */
struct LIBSPADES_PACKED PacketProgressBar {
	uint8_t packetID; /**< 22 `(Client<--Server)` */
	uint8_t entityID;
	uint8_t capturingTeam;
	int8_t rate;
	float progress;
};

/** Informs the client that a player has captured their enemy's intel. @ingroup packets */
struct LIBSPADES_PACKED PacketIntelCapture {
	uint8_t packetID; /**< 23 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player who has captured the intel. */
	uint8_t winning;  /**< Whether the capture was the last needed to end
	                       the game. Slightly useless considering the client can
	                       tell just by checking the total number of captures
	                       against the max captures itself. */
};

/** Informs the client that a player has picked up their enemy's intel. @ingroup packets */
struct LIBSPADES_PACKED PacketIntelPickup {
	uint8_t packetID; /**< 24 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player who has picked up the intel. */
};

/** Informs the client that a player has dropped their enemy's intel. @ingroup packets */
struct LIBSPADES_PACKED PacketIntelDrop {
	uint8_t packetID;      /**< 25 `(Client<--Server)` */
	uint8_t playerID;      /**< The ID of the player who has dropped the intel. */
	fvec3p pos;            /**< The position of the intel. */
};

/** Informs the client that a player has restocked. @ingroup packets */
struct LIBSPADES_PACKED PacketRestock {
	uint8_t packetID; /**< 26 `(Client<--Server)` */
	uint8_t playerID; /**< The ID of the player who has restocked. */
};

/** Changes the color of the fog. @ingroup packets */
struct LIBSPADES_PACKED PacketFogColor {
	uint8_t packetID;
	/**< 27 `(Client<--Server*)`
	 * @piquebug *Piqueserver allows players to send this packet for some reason, which will work if the /fog
	 * command is authorized.@n
	 */
	uint8_t a; /**< The alpha colour value of the fog. @note This parameter is unused. */
	color color;
};

/** Informs the client that a player has reloaded their weapon. @ingroup packets
 * For players other than the client player, this is usually used to
 * play sounds.
 */
struct LIBSPADES_PACKED PacketWeaponReload {
	uint8_t packetID;     /**< 28 `(Client<->Server)` */
	uint8_t playerID;     /**< The ID of the player who has reloaded. */
	uint8_t magazineAmmo; /**< The magazine (in-gun) ammunition count of the player who has reloaded. */
	uint8_t reserveAmmo;  /**< The reserve ammunition count of the player who has reloaded. */
};

/** Changes a player's team. @ingroup packets */
struct LIBSPADES_PACKED PacketChangeTeam {
	uint8_t packetID;
	/**< 29 `(Client-->Server*)`
	 * *OpenSpades supports receiving this packet, just with a very glitchy outcome.
	 */
	uint8_t playerID; /**< The ID of the player who is changing team. */
	uint8_t team;
	/**< The team the player is changing to. This may be 0 for the first team,
	 * 1 for the second, and -1 for spectator.
	 */
};

/** Changes a player's weapon. @ingroup packets */
struct LIBSPADES_PACKED PacketChangeWeapon {
	uint8_t packetID;
	/**< 30 `(Client<->Server*)`
	 * @piquebug *As a complete and utter showing of pyspades' (and therefore PySnip's and piqueserver's)
	 * excellence, the only reason the server is capable of sending this packet is because it doesn't have a
	 * reason. All packets of this type sent from the server can be safely ignored.
	 */
	uint8_t playerID; /**< The ID of the player who is changing weapon. */
	uint8_t weapon;
	/**< The weapon the player is changing to. Possible values are:
	 * - WeaponTypeRifle = 0,
	 * - WeaponTypeSMG = 1,
	 * - WeaponTypeShotgun = 2
	 */
};

/** From protocol v0.76 @ingroup packets
 * @todo document
 */
struct LIBSPADES_PACKED PacketMapCached {
	uint8_t packetID; /**< 31 `(Client-->Server)` */
	uint8_t cached;   /**< Whether the map is stored in the client's cache or not. */
};

/* extension packet structs */

/** Extension entries for @ref PacketExtensionInfo */
struct LIBSPADES_PACKED ExtensionInfoEntry {
	uint8_t extensionID;      /**< The extension's ID. */
	uint8_t extensionVersion; /**< The extension's version */
};

/** Tells the peer (client or server) about supported extensions to the AoS protocol. @ingroup packets
 * All extensions known to libspades can be found
 * in the @ref ExtensionID enum and the @ref packetByID page for now.
 */
struct LIBSPADES_PACKED PacketExtensionInfo {
	uint8_t packetID;                                   /**< 60 `(Client<->Server)` */
	uint8_t length;                                     /**< The amount of supported extensions. */
	struct ExtensionInfoEntry entries[VARIABLE_LENGTH]; /**< The list of supported extensions and their versions.
	                                                                                 Variable length. */
};

/** A packet that must usually be answered with @ref PacketHandShakeReturn to gain access to extensions. @ingroup packets
 * @note This packet and the return are non-existent in SpadesX, because the
 * maintainer said he "found them not needed".
 * @note See piqueserver
 * [#289](https://github.com/piqueserver/piqueserver/issues/289).
 */
struct LIBSPADES_PACKED PacketHandShakeInit {
	uint8_t packetID;   /**< 31 `(Client<--Server)` */
	uint32_t challenge; /**< 4 bytes that must be copied over to the return packet.
	                                           It is unknown why this would be necessary. */
};

/** The packet used to answer @ref PacketHandShakeInit. @ingroup packets
 * @note This packet and the init are non-existent in SpadesX, because the
 * maintainer said he "found them not needed".
 * @note See piqueserver
 * [#289](https://github.com/piqueserver/piqueserver/issues/289).
 */
struct LIBSPADES_PACKED PacketHandShakeReturn { /* just a copy of PacketHandShakeInit but whatever */
	uint8_t packetID;                       /**< 32 `(Client-->Server)` */
	uint32_t challenge;                     /**< 4 bytes that must be copied over from the init packet.
	                                                           It is unknown why this would be necessary. */
};

/** Requests client information from the client if the @ref
 * PacketHandShakeInit "Hand Shake" was completed (if supported). @ingroup packets
 * The information may be sent in @ref PacketVersionResponse, but it
 * is not required.
 * @note This packet is actually pretty useless,
 * considering the client can send the response whenever it wants as many
 * times as it wants (at least on piqueserver). In a perfect world, it would
 * be sent right after the hand shake is returned, or just be the handshake!
 */
struct LIBSPADES_PACKED PacketVersionRequest {
	uint8_t packetID; /**< 33 `(Client<--Server)` */
};

/** Informs the server about the details of the client. @ingroup packets
 * The standard format for the /client command:
 * `"(You are|PLAYER is) connected with CLIENT 4.16.128 on OPERATING
 * SYSTEM"`, where PLAYER is the player name, CLIENT is the string associated
 * with the client char, 4.16.256 is replaced with
 * versionMajor.versionMinor.versionRevision and OPERATING SYSTEM is replaced
 * with operatingSystemInfo
 *
 * @piquebug This packet can be sent at any time (even long after a @ref
 * PacketVersionRequest "Version Request") and any quantity of times.
 */
struct LIBSPADES_PACKED PacketVersionResponse {
	uint8_t packetID; /**< 34 `(Client-->Server)` */
	char client;
	/**< A single byte used to identify the client.
	 * It is unknown why this is a single byte and not a string like
	 * operatingSystemInfo. Commonly recognized values:
	 * - 'o': "OpenSpades",
	 * - 'B': "BetterSpades",
	 * - 'a': "ACE" (not present in SpadesX)
	 * - default: "Unknown(C)" where C is the character of this parameter
	 */
	uint8_t versionMajor;    /**< The major version of the client */
	uint8_t versionMinor;    /**< The minor version of the client */
	uint8_t versionRevision; /**< The revision of the client */
	char operatingSystemInfo[VARIABLE_LENGTH];
	/**<
	 * A string intended to contain the operating system the client is running
	 * on. Variable length.
	 * @note It does not actually need to contain the operating system,
	 * and can contain things like the client's name (because for some reason
	 * that is just a single char) or what method the client was acquired (like
	 * AppImage, from source, pre-compiled binary) for example.
	 *
	 * @note SpadesX may refuse to accept this parameter if it is greater than
	 * or equal to 256 bytes in length and result in the parameter being
	 * replaced with "Unknown". Do also note that setting the last byte to a
	 * null byte is not necessary, as SpadesX automatically does that when the
	 * packet is read.
	 *
	 * @piquebug Sending an extremely large size for this parameter
	 * (there is a limit somewhere below 32 MiB) will cause the server to
	 * temporarily freeze, disconnecting every player connected. Tested with
	 * 16777216 bytes of data.
	 */
};

/** Informs the client about stats of a player. @ingroup packets */
struct LIBSPADES_PACKED PacketPlayerProperties {
	uint8_t packetID;     /**< 64 */
	uint8_t subPacketID;  /**< 0 `(Client<--Server)` */
	uint8_t playerID;     /**< The ID of the player to inform the client about. */
	uint8_t HP;           /**< The HP of the player. */
	uint8_t blocks;       /**< The amount of blocks the player has. */
	uint8_t grenades;     /**< The amount of grenades the player has. */
	uint8_t magazineAmmo; /**< The amount of magazine ammunition the player has. */
	uint8_t reserveAmmo;  /**< The amount of reserve ammunition the player has. */
	uint32_t score;       /**< The score the player has. */
};

/** Requests the client to start an authentication transaction. @ingroup packets
 * The server MUST NOT allow the client to skip ahead in the transaction, or start it by itself.
 * Looking at you, piqueserver.
 */
struct LIBSPADES_PACKED PacketRequestAuthentication {
	uint8_t packetID;    /**< 65 */
	uint8_t subPacketID; /**< 0 `(Client<--Server)` */
};

/** Ends the authentication transaction and tells the client its permission level. @ingroup packets */
struct LIBSPADES_PACKED PacketEndAuthentication {
	uint8_t packetID;    /**< 65 */
	uint8_t subPacketID; /**< 1 `(Client<--Server)` */
	uint8_t success;     /**< Equal to 1 if the client was successfully authenticated. Equal to 0 otherwise. */
	char permissionLevel[VARIABLE_LENGTH]; /**< NULL-terminated string encoded with UTF-8 that contains the
	                                          permission level granted. For example, "Player", "Trusted", "Admin"
	                                        */
};

/** Sends the clients's public key to the server in response to the request for authentication. @ingroup packets */
struct LIBSPADES_PACKED PacketSendPublicKey {
	uint8_t packetID;      /**< 65 */
	uint8_t subPacketID;   /**< 2 `(Client-->Server)` */
	uint8_t publicKey[32]; /**< Ed25519 public key to be used for authentication. */
};

/** Sends the client a nonce to sign after receiving its public key. @ingroup packets
 * The nonce MUST NEVER be re-used, and should be from a secure random source.
 * At least 32 bytes is recommended.
 */
struct LIBSPADES_PACKED PacketSendNonce {
	uint8_t packetID;               /**< 65 */
	uint8_t subPacketID;            /**< 3 `(Client<--Server)` */
	uint8_t nonce[VARIABLE_LENGTH]; /**< Data to be signed with the client's secret key. */
};

/** Sends the server back the nonce sent the client, signed with the client's secret key. @ingroup packets */
struct LIBSPADES_PACKED PacketSendSignature {
	uint8_t packetID;      /**< 65 */
	uint8_t subPacketID;   /**< 4 `(Client-->Server)` */
	uint8_t signature[64]; /**< The nonce sent by the server, signed with the client's secret key. */
};

#ifndef PACK_GCC
#pragma pack(pop)
#endif

enum PacketType {
	PacketTypePositionData = 0,      /* Client<->Server */
	PacketTypeOrientationData = 1,   /* usually Client-->Server, but Client<->Server is possible */
	PacketTypeWorldUpdate = 2,       /* Client<--Server */
	PacketTypeInput = 3,             /* Client<->Server */
	PacketTypeWeaponInput = 4,       /* Client<->Server */
	PacketTypeHit = 5,               /* Client-->Server */
	PacketTypeSetHP = 5,             /* Client<--Server */
	PacketTypeGrenade = 6,           /* Client<->Server */
	PacketTypeSetTool = 7,           /* Client<->Server */
	PacketTypeSetColor = 8,          /* Client<->Server */
	PacketTypeExistingPlayer = 9,    /* Client<->Server */
	PacketTypeShortPlayerData = 10,  /* Client---Server */
	PacketTypeMoveObject = 11,       /* Client<--Server */
	PacketTypeCreatePlayer = 12,     /* Client<--Server */
	PacketTypeBlockAction = 13,      /* Client<->Server */
	PacketTypeBlockLine = 14,        /* Client<->Server */
	PacketTypeStateData = 15,        /* Client<--Server */
	PacketTypeKill = 16,             /* Client<--Server */
	PacketTypeChatMessage = 17,      /* Client<->Server */
	PacketTypeMapStart = 18,         /* Client<--Server */
	PacketTypeMapChunk = 19,         /* Client<--Server */
	PacketTypePlayerLeft = 20,       /* Client<--Server */
	PacketTypeTerritoryCapture = 21, /* Client<--Server */
	PacketTypeProgressBar = 22,      /* Client<--Server */
	PacketTypeIntelCapture = 23,     /* Client<--Server */
	PacketTypeIntelPickup = 24,      /* Client<--Server */
	PacketTypeIntelDrop = 25,        /* Client<--Server */
	PacketTypeRestock = 26,          /* Client<--Server */
	PacketTypeFogColor = 27,         /* Client<--Server */
	PacketTypeWeaponReload = 28,     /* Client<->Server */
	PacketTypeChangeTeam = 29,       /* Client-->Server */
	PacketTypeChangeWeapon = 30,     /* Client<->Server */
	PacketTypeMapCached = 31,        /* Client-->Server */

	/* extension packets. NOTE: for most servers, you must complete the
	   HandShakeInit and Return to gain access to extensions. */
	PacketTypeHandShakeInit = 31,   /* Client<--Server */
	PacketTypeHandShakeReturn = 32, /* Client-->Server */
	PacketTypeVersionRequest = 33,
	/* Client<--Server */ /* sent before extension info is provided */
	PacketTypeVersionResponse = 34,
	/* Client-->Server */ /* sendable before extension info is provided */

	PacketTypeExtensionInfo = 60, /* Client<->Server */

	PacketTypePlayerProperties = 64,
	PacketTypeEd25519Authentication = 65
	/* I wonder if it would just be better to have all the subpackets for this? */
};

enum SubPacketType {
	/* PacketTypePlayerProperties */
	SubPacketTypePlayerProperties = 0, /* Client<--Server */

	/* PacketTypeEd25519Authentication */
	SubPacketTypeRequestAuthentication = 0, /* Client<--Server */
	SubPacketTypeEndAuthentication = 1,     /* Client<--Server */
	SubPacketTypeSendPublicKey = 2,         /* Client-->Server */
	SubPacketTypeSendNonce = 3,             /* Client<--Server */
	SubPacketTypeSendSignature = 4          /* Client-->Server */
};

enum WeaponType { WeaponTypeRifle = 0, WeaponTypeSMG = 1, WeaponTypeShotgun = 2 };

enum BlockActionType {
	BlockActionTypeBuild = 0,
	BlockActionTypeSpadeGunDestroy = 1,
	BlockActionTypeSpadeSecondaryDestroy = 2,
	BlockActionTypeGrenadeDestroy = 3
};

enum HurtType { HurtTypeFall = 0, HurtTypeWeapon = 1 };

enum ChatType { ChatTypeAll = 0, ChatTypeTeam = 1, ChatTypeSystem = 2 };

/* Might or might not want to use the defines instead in new things. Probably doesn't matter too much.
 * The defines are there since enum uses signed int, which does not work for the CRC32.
 */
enum ProtocolVersion { ProtocolVersion75 = 3, ProtocolVersion76 = 4 };

#define LIBSPADES_PROTOCOL_VERSION_10     0
#define LIBSPADES_PROTOCOL_VERSION_21     0
#define LIBSPADES_PROTOCOL_VERSION_22     0
/* Most of these are a CRC32 of client.exe */
#define LIBSPADES_PROTOCOL_VERSION_26     0x40950dbb
#define LIBSPADES_PROTOCOL_VERSION_30     0x7965ce91
#define LIBSPADES_PROTOCOL_VERSION_31     0x7965ce91
#define LIBSPADES_PROTOCOL_VERSION_32     0x7a8660d4
#define LIBSPADES_PROTOCOL_VERSION_33     0x709cc6e3
#define LIBSPADES_PROTOCOL_VERSION_35     0xf39348d7
#define LIBSPADES_PROTOCOL_VERSION_36     0x4da4f41e
#define LIBSPADES_PROTOCOL_VERSION_40     0xa3937cd0
#define LIBSPADES_PROTOCOL_VERSION_41     0xa3937cd0
#define LIBSPADES_PROTOCOL_VERSION_42     0x09e3c405
#define LIBSPADES_PROTOCOL_VERSION_46     0x0e240254
#define LIBSPADES_PROTOCOL_VERSION_47     0x9217d00e
#define LIBSPADES_PROTOCOL_VERSION_48     0x187532bf
#define LIBSPADES_PROTOCOL_VERSION_49     0xb2aa66f5
#define LIBSPADES_PROTOCOL_VERSION_50     0x1fcb39d1
#define LIBSPADES_PROTOCOL_VERSION_51     0x679b3c4d
#define LIBSPADES_PROTOCOL_VERSION_52     0x9c8791f8
#define LIBSPADES_PROTOCOL_VERSION_53     0x2aecbe65
#define LIBSPADES_PROTOCOL_VERSION_54     0x4e849539
#define LIBSPADES_PROTOCOL_VERSION_55     0x20791226
#define LIBSPADES_PROTOCOL_VERSION_58     0xad004b4e
#define LIBSPADES_PROTOCOL_VERSION_60_009 0x75894111
#define LIBSPADES_PROTOCOL_VERSION_60_010 0xc7e8dab2
#define LIBSPADES_PROTOCOL_VERSION_60_011 0xea5e9039
#define LIBSPADES_PROTOCOL_VERSION_60_014 0x607521b0
#define LIBSPADES_PROTOCOL_VERSION_60     LIBSPADES_PROTOCOL_VERSION_60_014
#define LIBSPADES_PROTOCOL_VERSION_61_004 0x16165bae
#define LIBSPADES_PROTOCOL_VERSION_61_005 0x8ab03b5f
#define LIBSPADES_PROTOCOL_VERSION_61_006 0x9e592202
#define LIBSPADES_PROTOCOL_VERSION_61     LIBSPADES_PROTOCOL_VERSION_61_006
#define LIBSPADES_PROTOCOL_VERSION_62_003 0x13575615
#define LIBSPADES_PROTOCOL_VERSION_62_006 0x3da3f698
#define LIBSPADES_PROTOCOL_VERSION_62     LIBSPADES_PROTOCOL_VERSION_62_006
#define LIBSPADES_PROTOCOL_VERSION_70_008 0x9f4fe987
#define LIBSPADES_PROTOCOL_VERSION_70_009 0x7a4139b6
#define LIBSPADES_PROTOCOL_VERSION_70_012 0xb0525b7e
/* No idea where this number came from. Not a CRC32 of that version's client.exe, anyway. */
#define LIBSPADES_PROTOCOL_VERSION_70_017 0xdc05cbda
#define LIBSPADES_PROTOCOL_VERSION_70     LIBSPADES_PROTOCOL_VERSION_70_017
#define LIBSPADES_PROTOCOL_VERSION_75_003 1
#define LIBSPADES_PROTOCOL_VERSION_75_004 1
#define LIBSPADES_PROTOCOL_VERSION_75_007 1
#define LIBSPADES_PROTOCOL_VERSION_75_009 1
#define LIBSPADES_PROTOCOL_VERSION_75_010 2
#define LIBSPADES_PROTOCOL_VERSION_75_011 2
#define LIBSPADES_PROTOCOL_VERSION_75_013 3
#define LIBSPADES_PROTOCOL_VERSION_75_015 3
#define LIBSPADES_PROTOCOL_VERSION_75     LIBSPADES_PROTOCOL_VERSION_75_015
#define LIBSPADES_PROTOCOL_VERSION_76_010 4
#define LIBSPADES_PROTOCOL_VERSION_76     LIBSPADES_PROTOCOL_VERSION_76_010

enum ToolType {
	ToolTypeSpade = 0,
	ToolTypeBlock = 1,
	ToolTypeGun = 2,
	ToolTypeGrenade = 3
};

enum KillType {
	KillTypeWeapon = 0,
	KillTypeHeadshot = 1,
	KillTypeMelee = 2,
	KillTypeGrenade = 3,
	KillTypeFall = 4,
	KillTypeTeamChange = 5,
	KillTypeWeaponChange = 6
};

enum WeaponInputType { WeaponInputTypePrimary = 1, WeaponInputTypeSecondary = 2 };

enum HitType { HitTypeTorso = 0, HitTypeHead = 1, HitTypeArms = 2, HitTypeLegs = 3, HitTypeMelee = 4 };

enum KeyStateType {
	KeyStateTypeForward = 1,
	KeyStateTypeBackward = 2,
	KeyStateTypeLeft = 4,
	KeyStateTypeRight = 8,
	KeyStateTypeJump = 16,
	KeyStateTypeCrouch = 32,
	KeyStateTypeSneak = 64,
	KeyStateTypeSprint = 128
};

/** Contains the IDs of all extensions known to libspades. @todo explain why
 * packetless extensions exist, really just document them in general */
enum ExtensionID {
	/* Extensions with packets */
	ExtensionIDPlayerProperties = 0,      /**< See @ref PacketPlayerProperties and
	                                                                             [#35](https://github.com/piqueserver/aosprotocol/issues/35)
	                                       */
	ExtensionIDEd25519Authentication = 1, /**< My own extension to replace passwords and the garbage plaintext
	                                         /login command commonly used with them. :D */

	/* Extensions without packets */
	ExtensionID256Players = 192,   /**< Enables support for up to 256 max players. */
	ExtensionIDMessageTypes = 193, /**< Provides more chat types. See
	                                                [#14](https://github.com/piqueserver/aosprotocol/issues/14) */
	ExtensionIDKickReason = 194    /**< Shows the last sent chat meassage upon disconnect. See
	                                                                           [#15](https://github.com/piqueserver/aosprotocol/issues/15)
	                 */
};

enum MagazineAmmoSize { MagazineAmmoSizeRifle75 = 10, MagazineAmmoSizeSMG75 = 30, MagazineAmmoSizeShotgun75 = 6 };

enum ReserveAmmoSize { ReserveAmmoSizeRifle75 = 50, ReserveAmmoSizeSMG75 = 120, ReserveAmmoSizeShotgun75 = 48 };

enum FireDelayMilliseconds {
	FireDelayMillisecondsRifle75 = 500,
	FireDelayMillisecondsSMG75 = 100,
	FireDelayMillisecondsShotgun75 = 1000
};

enum PelletQuantity { PelletQuantityRifle75 = 1, PelletQuantitySMG75 = 1, PelletQuantityShotgun75 = 8 };

enum GamemodeType { GamemodeTypeCTF = 0, GamemodeTypeTC = 1 };

#ifdef __cplusplus
}
#endif
#endif
