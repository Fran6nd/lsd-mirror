#ifndef PYPVX_DEMONCORE_H
#define PYPVX_DEMONCORE_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "state.h"

/* TODO: if i throw a grenade on bubble tower blue team and change to red, does it destroy blue team blocks? */
struct Grenade {
	clk detonateTime;
	fvec3 pos;
	fvec3 vel;
	uint8_t pid;
	uint8_t team;
	uint8_t exists;
};

int clip_phys(float ox, float oy, float oz, Map map, int wrap);
int cast2(Map map, float startX, float startY, float startZ, float endX, float endY, float endZ, float length, int32_t *x, int32_t *y, int32_t *z, int last);
int cast_ray2(Map map, fvec3p start, fvec3p end);
void dcore_block_line(int32_t startX, int32_t startY, int32_t startZ, int32_t endX, int32_t endY, int32_t endZ, Map map, const uint8_t *color);
void change_crouch(int crouching, struct Player *p, Map map, int wrap);
int change_crouch_me(int crouching, struct Player *p, Map map, int wrap);
int32_t move_player(struct Player *p, float secondsSinceLastUpdate, Map map, int wrap);
int move_grenade(struct Grenade *grenade, float secondsSinceLastUpdate, Map map);

#endif
