#ifndef PYPVX_DEMONCORE_H
#define PYPVX_DEMONCORE_H

#include <stddef.h>
#include "protocol.h"
#include "bitmask.h"
#include <stdint.h>

typedef fvec3p fvec3;
/*
typedef struct {
	float x;
	float y;
	float z;
} fvec3;*/
struct Vector {float x, y, z;};
struct Vector32 {int32_t x, y, z;};
struct Vector32u {uint32_t x, y, z;};

typedef uint64_t clk;
struct Player;
#ifdef REDEF_PLAYER
struct Player {
	fvec3 position;
	fvec3 orientation;
	fvec3 velocity;
	uint8_t inputs;
	uint8_t mouseInputs;
	uint8_t item;
	int wade;
	int airborne;
	int connected;
	int alive;
	char name[16];
	uint8_t team;
	uint8_t blockColor[3];
};
#endif
/* TODO: if i throw a grenade on bubble tower blue team and change to red, does it destroy blue team blocks? */
struct Grenade {
	clk detonateTime;
	fvec3 pos;
	fvec3 vel;
	uint8_t pid;
	uint8_t team;
	uint8_t exists;
};

int clip_player(float ox, float oy, float oz, const uint8_t *solidData, int wrap);
int can_see(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1, float z1);
int cast_ray(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1, float z1, float length, int32_t *x, int32_t *y, int32_t *z);
int cast_ray2(const uint8_t *solidData, fvec3p start, fvec3p end);
void block_line(int32_t startX, int32_t startY, int32_t startZ, int32_t endX, int32_t endY, int32_t endZ, struct BitmaskUData *map, const uint8_t *color);
void change_crouch(int crouching, struct Player *p, const uint8_t *solidData, int wrap);
int change_crouch_me(int crouching, struct Player *p, const uint8_t *solidData, int wrap);
int32_t move_player(struct Player *p, float secondsSinceLastUpdate, const uint8_t *solidData, int wrap);
int move_grenade(struct Grenade *grenade, float secondsSinceLastUpdate, const uint8_t *solidData, int correct);

#endif
