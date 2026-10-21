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

struct Cast {
	int remaining;
	ivec3 vox;
	ivec3 step;
	fvec3 side_dist;
	fvec3 delta;
};

struct BlockLine {
	int remaining;
	ivec3 vox;
	ivec3 step;
	ivec3 side_dist;
	ivec3 delta;
};

struct Cast cast_start(fvec3 start, fvec3 end);
int cast_step(struct Cast *cast);

int cast_ray(Map map, fvec3 start, fvec3 end, ivec3 *vox);
int cast_ray_last(Map map, fvec3 start, fvec3 end, ivec3 *vox);

struct BlockLine block_line_start(ivec3 start, ivec3 end);
int block_line_step(struct BlockLine *line);

void block_line(Map map, ivec3 start, ivec3 end, color color);

int clip_phys(float ox, float oy, float oz, Map map, int wrap);

void change_crouch(int crouching, struct Player *p, Map map, int wrap);
int change_crouch_me(int crouching, struct Player *p, Map map, int wrap);
int32_t move_player(struct Player *p, float secondsSinceLastUpdate, Map map, int wrap);
int move_grenade(struct Grenade *grenade, float secondsSinceLastUpdate, Map map);

#endif
