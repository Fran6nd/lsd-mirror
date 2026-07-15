/*
	Copyright (c) Mathias Kaerlev 2011-2012.

	This file is part of pyspades.

	pyspades is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pyspades is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pyspades.  If not, see <http://www.gnu.org/licenses/>.

*/

/* "world_c.cpp - this shit is hazardous" -- matpow2, 06475d84 */

#define REDEF_PLAYER
#include "demoncore.h"
#include <math.h>
#include <stdlib.h>
#include "bitmask.h"

#define MAX(x,y) ((x)>(y) ? (x) : (y))

struct Vector {float x, y, z;};
struct Vector32 {int32_t x, y, z;};
struct Vector32u {uint32_t x, y, z;};

typedef struct Vector Vector;
typedef struct Vector32 Vector32;
typedef struct Vector32u Vector32u;

#define FALL_SLOW_DOWN 0.24f
#define FALL_DAMAGE_VELOCITY 0.58f
#define FALL_DAMAGE_SCALAR 4096

static float fmodulof(float x, float y) {
	x = fmodf(x, y);
	if (x < 0)
		return x + y;

	return x;
}

static int phys_solid(float x, float y, float z, const uint8_t *solidData) {
	if (z < 0)
		return 0;
	if (z >= 64)
		return 1;

	return pvx_voxel_get_solidity4(solidData, CALC_I(x, y), z);
}

int clip_phys(float x, float y, float z, const uint8_t *solidData, int wrap) {
	x = floorf(x);
	y = floorf(y);
	z = floorf(z);

	if (wrap) {
		x = fmodulof(x, 512);
		y = fmodulof(y, 512);
	}

	if (x < 0 || x >= 512 || y < 0 || y >= 512)
		return 1;

	if (z == 63)
		z = 62;

	return phys_solid(x, y, z, solidData);
}

static int clip_grenade_explode(float x, float y, float z, const uint8_t *solidData) {
	x = fmodulof(floorf(x), 512);
	y = fmodulof(floorf(y), 512);
	z = floorf(z);

	return phys_solid(x, y, z, solidData);
}

/* Hey, this raycaster actually works properly! */
/* Relevant:
 * https://www.cs.yorku.ca/~amana/research/grid.pdf
 * https://github.com/fenomas/fast-voxel-raycast
 * https://voxel.wiki/wiki/raytracing/
 * https://voxel.wiki/wiki/raycasting/
 */
int cast2(const uint8_t *solidData, float startX, float startY, float startZ, float endX, float endY,
                float endZ, float length, int32_t *x, int32_t *y, int32_t *z, int last) {
	int stepx, stepy, stepz;
	float offx, offy, offz;
	float deltax, deltay, deltaz;
	float tmaxx, tmaxy, tmaxz;
	float distx, disty, distz;
	(void)length;

	/* Position of endX relative to startX */
	offx = endX - startX;
	offy = endY - startY;
	offz = endZ - startZ;

	/* Direction in each axis the ray travels */
	stepx = offx < 0 ? -1 : 1;
	stepy = offy < 0 ? -1 : 1;
	stepz = offz < 0 ? -1 : 1;

	deltax = fabsf(1 / offx);
	deltay = fabsf(1 / offy);
	deltaz = fabsf(1 / offz);

	/* This magic thing prevents off-by-one errors. */
	distx = offx < 0 ? ceilf(startX) - startX - 1 : floorf(startX) - startX + 1;
	disty = offy < 0 ? ceilf(startY) - startY - 1 : floorf(startY) - startY + 1;
	distz = offz < 0 ? ceilf(startZ) - startZ - 1 : floorf(startZ) - startZ + 1;

	tmaxx = offx == 0 ? HUGE_VAL : distx / offx;
	tmaxy = offy == 0 ? HUGE_VAL : disty / offy;
	tmaxz = offz == 0 ? HUGE_VAL : distz / offz;

	int32_t taxilen;
	int32_t voxx = floorf(startX), voxy = floorf(startY), voxz = floorf(startZ);
	int32_t lastvoxx = voxx, lastvoxy = voxy, lastvoxz = voxz;
	taxilen = abs(voxx - (int32_t)floorf(endX)) + abs(voxy - (int32_t)floorf(endY)) + abs(voxz - (int32_t)floorf(endZ));
	while (1) {
		if (stepz == -1 && voxz < 0)
			return 0;

		if (clip_grenade_explode(voxx, voxy, voxz, solidData)) {
			if (last) {
				*x = lastvoxx;
				*y = lastvoxy;
				*z = lastvoxz;
			} else {
				*x = voxx;
				*y = voxy;
				*z = voxz;
			}
			return 1;
		}

		if (taxilen-- == 0)
			return 0;

		lastvoxx = voxx;
		lastvoxy = voxy;
		lastvoxz = voxz;

		/* TODO: just <, not <=? */
		if (tmaxz <= tmaxx && tmaxz <= tmaxy) {
			voxz += stepz;
			tmaxz += deltaz;
			/* TODO: this is not horiz */
		} else if (tmaxx < tmaxy) {
			voxx += stepx;
			tmaxx += deltax;
		} else {
			voxy += stepy;
			tmaxy += deltay;
		}
	}
}

int cast_ray2(const uint8_t *solidData, fvec3p start, fvec3p end) {
	int32_t trash;
	return cast2(solidData, start.x, start.y, start.z, end.x, end.y, end.z, 0, &trash, &trash, &trash, 0);
}

void dcore_block_line(int32_t startX, int32_t startY, int32_t startZ, int32_t endX, int32_t endY, int32_t endZ, struct BitmaskUData *map, const uint8_t *color) {
	/* d. . . di. . . diamonds?! */
	Vector32u off, d, di;
	Vector32 cursor, step;
	uint32_t maxoff;

	/* End pos's offset relative to the start position. */
	off.x = abs(endX - startX);
	off.y = abs(endY - startY);
	off.z = abs(endZ - startZ);

	maxoff = MAX(MAX(off.x, off.y), off.z);

	/* Axes which don't have any offset don't need to be stepped in. Therefore, some really high value is assigned here to ignore them. */
	di.x = off.x == 0 ? (uint32_t)-1 : maxoff * 1024 / off.x;
	di.y = off.y == 0 ? (uint32_t)-1 : maxoff * 1024 / off.y;
	di.z = off.z == 0 ? (uint32_t)-1 : maxoff * 1024 / off.z;

	d.x = di.x / 2;
	d.y = di.y / 2;
	d.z = di.z / 2;

	/* Each iteration of the loop, an axis is chosen to step in. It has to be negative or positive, of course. */
	step.x = endX < startX ? -1 : 1;
	step.y = endY < startY ? -1 : 1;
	step.z = endZ < startZ ? -1 : 1;

	/* Slightly tampers with rounding. I should probably see exactly how. */
	if (step.x >= 0)
		d.x = di.x - d.x;
	if (step.y >= 0)
		d.y = di.y - d.y;
	if (step.z >= 0)
		d.z = di.z - d.z;

	cursor.x = startX;
	cursor.y = startY;
	cursor.z = startZ;

	while (1) {
		size_t i = CALC_I(cursor.x, cursor.y);

		if (!pvx_voxel_get_solidity4(map->solidData, i, cursor.z)) {
			pvx_voxel_create4(map->solidData, i, cursor.z);
			pvx_voxel_color5(map->colorData, color, i, cursor.z);
		}

		if (cursor.x == endX && cursor.y == endY && cursor.z == endZ)
			return;

		if (d.z <= d.x && d.z <= d.y) {
			cursor.z += step.z;
			d.z += di.z;
		} else if (d.x < d.y) {
			cursor.x += step.x;
			d.x += di.x;
		} else {
			cursor.y += step.y;
			d.y += di.y;
		}
	}
}

void try_uncrouch(struct Player *p, const uint8_t *solidData, int wrap) {
	float x1 = p->pos.x + 0.45f;
	float x2 = p->pos.x - 0.45f;
	float y1 = p->pos.y + 0.45f;
	float y2 = p->pos.y - 0.45f;
	float z1 = p->pos.z + 2.25f;

	/* First check if player can lower feet if in midair. */
	if (p->airborne && !(clip_phys(x1, y1, z1, solidData, wrap) || clip_phys(x1, y2, z1, solidData, wrap) || clip_phys(x2, y1, z1, solidData, wrap) || clip_phys(x2, y2, z1, solidData, wrap)))
		return;

	/* By the time this has been reached, the player cannot lower feet, so the best option is to raise the head instead. */
	p->pos.z -= 0.9f;
}

int try_uncrouch_me(struct Player *p, const uint8_t *solidData, int wrap) {
	float x1 = p->pos.x + 0.45f;
	float x2 = p->pos.x - 0.45f;
	float y1 = p->pos.y + 0.45f;
	float y2 = p->pos.y - 0.45f;
	float z1 = p->pos.z + 2.25f;
	float z2 = p->pos.z - 1.35f;

	/* First check if player can lower feet if in midair. */
	if (p->airborne && !(clip_phys(x1, y1, z1, solidData, wrap) || clip_phys(x1, y2, z1, solidData, wrap) || clip_phys(x2, y1, z1, solidData, wrap) || clip_phys(x2, y2, z1, solidData, wrap)))
		return 1;

	/* By the time this has been reached, the player cannot lower feet, so raise the head instead if possible. */
	if (!(clip_phys(x1, y1, z2, solidData, wrap) || clip_phys(x1, y2, z2, solidData, wrap) || clip_phys(x2, y1, z2, solidData, wrap) || clip_phys(x2, y2, z2, solidData, wrap))) {
		p->pos.z -= 0.9f;
		return 1;
	}

	/* Nothing seemed possible. */
	return 0;
}

/* Don't call this unless crouch state is actually changing. */
void change_crouch(int crouching, struct Player *p, const uint8_t *solidData, int wrap) {
	if (crouching && !p->airborne)
		p->pos.z += 0.9;
	else
		try_uncrouch(p, solidData, wrap);
}

int change_crouch_me(int crouching, struct Player *p, const uint8_t *solidData, int wrap) {
	if (crouching && !p->airborne) {
		p->pos.z += 0.9;
		return 1;
	}
	return try_uncrouch_me(p, solidData, wrap);
}

static int satan(float z, float zgreaterequal, float x1, float x2, float y1, float y2, float ztest, const uint8_t *solidData, int wrap) {
	for (; z >= zgreaterequal && !clip_phys(x1, y1, ztest + z, solidData, wrap) && !clip_phys(x2, y2, ztest + z, solidData, wrap); z -= 0.9);
	return z < zgreaterequal;
}

#define SATANIC_LOOP(zstart, zgreaterequal, x1, x2, y1, y2) satan(zstart, zgreaterequal, x1, x2, y1, y2, nz, solidData, wrap)
#define MYSTERY_LOOP_OVER_AXIS(axis, x1, x2, y1, y2) do {\
	nextp = f * p->vel.axis + p->pos.axis; \
	testp = nextp + copysignf(0.45, p->vel.axis); \
	if (SATANIC_LOOP(m, -1.36, x1, x2, y1, y2)) \
		p->pos.axis = nextp; \
	else if (!climb) { \
		if (canclimb && SATANIC_LOOP(0.35, -2.36, x1, x2, y1, y2)) { \
			p->pos.axis = nextp; \
			climb = 1; \
		} else \
			p->vel.axis = 0; \
	} \
} while (0);

/* Like the demon core, but for player movement. */
static void boxclipmove(struct Player *p, float secondsSinceLastUpdate, const uint8_t *solidData, int wrap) {
	float f = secondsSinceLastUpdate * 32;
	float nextp, testp;
	float nz;
	int canclimb = !(p->inputs & (KeyStateTypeCrouch | KeyStateTypeSprint)) && p->ori.z < 0.5;
	int climb = 0;

	float offset, m;
	if (p->inputs & KeyStateTypeCrouch) {
		offset = 0.45;
		m = 0.9;
	} else {
		offset = 0.9;
		m = 1.35;
	}

	nz = p->pos.z + offset;

	MYSTERY_LOOP_OVER_AXIS(x, testp, testp, p->pos.y - 0.45, p->pos.y + 0.45);
	MYSTERY_LOOP_OVER_AXIS(y, p->pos.x - 0.45, p->pos.x + 0.45, testp, testp);

	if (climb) {
		p->vel.x /= 2;
		p->vel.y /= 2;
		nz--;
		m = -1.35;
	} else {
		nz += p->vel.z * secondsSinceLastUpdate * 32;
		m = copysignf(m, p->vel.z);
	}

#define SCLIP(xoff, yoff) clip_phys(p->pos.x + (xoff), p->pos.y + (yoff), nz + m, solidData, wrap)
	/* secondsSinceLastUpdate not being 1/60 can screw with some calculations
	 * here (see nz). Makes players nice and jittery. Wonder if I can (should)
	 * force it, somehow, to 1/60 for just the Z axis? */
	p->airborne = 1;
	if (SCLIP(-0.45, -0.45) || SCLIP (-0.45, 0.45) || SCLIP(0.45, -0.45) || SCLIP(0.45, 0.45)) {
		if (p->vel.z >= 0) {
			p->wade = p->pos.z > 61;
			p->airborne = 0;
		}

		p->vel.z = 0;
	} else
		p->pos.z = nz - offset;
}

static float calc_acceleration(struct Player *p, float secondsSinceLastUpdate, int diagonal) {
	float acceleration = secondsSinceLastUpdate;

	/* Affect horizontal movement acceleration based on airborne/inputs. */
	if (p->airborne)
		acceleration *= 0.1;
	else if (p->inputs & KeyStateTypeCrouch)
		acceleration *= 0.3;
	else if ((p->mouseInputs & GunInputTypeSecondary && p->tool == ToolTypeGun) || p->inputs & KeyStateTypeSneak)
		acceleration *= 0.5;
	else if (p->inputs & KeyStateTypeSprint)
		acceleration *= 1.3;

	/* If moving diagonally, don't go double the speed. */
	if (diagonal)
#ifdef OPENSPADES_SQRT
		acceleration /= sqrt(2);
#else
		acceleration *= 0.70710678f;
#endif

	return acceleration;
}

/* Does physics and crunches bones when applicable. */
int32_t move_player(struct Player *p, float secondsSinceLastUpdate, const uint8_t *solidData, int wrap) {
	float horizontalHypotenuse, acceleration, friction, oldZVelocity, verticalMove, horizontalMove;
	Vector rightSide;

	/* Get the player's right side, but in 2D instead of 3D.
	 * Trivia time: notice how this is 2D, not 3D.
	 * This is why looking down and moving left/right is full speed, but moving forward/backward is not.*/
	horizontalHypotenuse = sqrtf(p->ori.x * p->ori.x + p->ori.y * p->ori.y);
	if (horizontalHypotenuse == 0)
		horizontalHypotenuse = 1;
	rightSide.x = -p->ori.y / horizontalHypotenuse;
	rightSide.y = p->ori.x / horizontalHypotenuse;

	/* Vertical/horizontal in a 2D sense. Most other instances of vertical refer to the axis
	 * which gravity operates along, whereas horizontal refers to the other two axes. */
	verticalMove = (p->inputs & KeyStateTypeForward) ? 1 : ((p->inputs & KeyStateTypeBackward) ? -1 : 0);
	horizontalMove = (p->inputs & KeyStateTypeLeft) ? -1 : ((p->inputs & KeyStateTypeRight) ? 1 : 0);
	acceleration = calc_acceleration(p, secondsSinceLastUpdate, verticalMove && horizontalMove);

	if ((p->inputs & KeyStateTypeJump)) {
		p->inputs &= ~KeyStateTypeJump;
		p->vel.z = -0.36;
	}

	/* Add to velocity in the direction player is trying to move. Do vertical air friction too. */
	p->vel.x += p->ori.x * acceleration * verticalMove;
	p->vel.y += p->ori.y * acceleration * verticalMove;
	p->vel.x += rightSide.x * acceleration * horizontalMove;
	p->vel.y += rightSide.y * acceleration * horizontalMove;
	p->vel.z += secondsSinceLastUpdate;
	/* All of this adding 1 seems like a fire hazard. */
	p->vel.z /= secondsSinceLastUpdate + 1;

	/* Horizontal friction, for water, air or ground. */
	friction = secondsSinceLastUpdate * (p->wade ? 6 : (p->airborne ? 1 : 4)) + 1;
	p->vel.x /= friction;
	p->vel.y /= friction;

	/* Call into the demon core. */
	oldZVelocity = p->vel.z;
	boxclipmove(p, secondsSinceLastUpdate, solidData, wrap);

	if (wrap) {
		p->pos.x = fmodf(p->pos.x, 512);
		p->pos.y = fmodf(p->pos.y, 512);
		if (p->pos.x < 0)
			p->pos.x += 512;
		if (p->pos.y < 0)
			p->pos.y += 512;
	}

	if (!(p->vel.z == 0 && (oldZVelocity > FALL_SLOW_DOWN)))
		return -1;

	p->vel.x /= 2;
	p->vel.y /= 2;

	if (oldZVelocity <= FALL_DAMAGE_VELOCITY)
		return 0;

	oldZVelocity -= FALL_DAMAGE_VELOCITY;

	/* Wouldn't want an integer to overflow. */
	if (oldZVelocity >= 724.0774)
		return 2147483647;

	return oldZVelocity * oldZVelocity * FALL_DAMAGE_SCALAR;
}

/* Returns 1 if the grenade collides with something, otherwise 0. */
int move_grenade(struct Grenade *g, float secondsSinceLastUpdate, const uint8_t *solidData) {
	Vector32 newPos;
	fvec3 fpos = g->pos;

	float velMultiplier = secondsSinceLastUpdate * 32;
	g->vel.z += secondsSinceLastUpdate;

	g->pos.x += g->vel.x * velMultiplier;
	g->pos.y += g->vel.y * velMultiplier;
	g->pos.z += g->vel.z * velMultiplier;

	newPos.x = floor(g->pos.x);
	newPos.y = floor(g->pos.y);
	newPos.z = floor(g->pos.z);

	if (!clip_phys(newPos.x, newPos.y, newPos.z, solidData, 1))
		return 0; /* Grenade does not bounce. It's still in the air. */
	else { /* Grenade *does* bounce. It smacked into something. */
		Vector32 oldPos;

		g->pos = fpos; /* Set back to old position */
		g->vel.x *= 0.36;
		g->vel.y *= 0.36;
		g->vel.z *= 0.36;

		oldPos.x = floor(g->pos.x);
		oldPos.y = floor(g->pos.y);
		oldPos.z = floor(g->pos.z);

		/* Determines which face the grenade collided with. It's only allowed to collide with one for some reason.
		 * You can tell since it uses both if/else instead of if and also explicitly forbids anything being different except for the face being tested.
		 * (newPos.a != oldPos.a && theOtherOnesAreUnchanged) || didNotTakeTimeToParseThisOne
		 * If two faces change, things turn funny. Reminds me of that corner bug that was mentioned in a comment I ripped out.
		 */
		if      (newPos.z != oldPos.z && ((newPos.x == oldPos.x && newPos.y == oldPos.y) || !clip_phys(newPos.x, newPos.y, oldPos.z, solidData, 1)))
			g->vel.z = -g->vel.z;
		else if (newPos.x != oldPos.x && ((newPos.y == oldPos.y && newPos.z == oldPos.z) || !clip_phys(oldPos.x, newPos.y, newPos.z, solidData, 1)))
			g->vel.x = -g->vel.x;
		else if (newPos.y != oldPos.y && ((newPos.x == oldPos.x && newPos.z == oldPos.z) || !clip_phys(newPos.x, oldPos.y, newPos.z, solidData, 1)))
			g->vel.y = -g->vel.y;

		return 1;
	}
}
