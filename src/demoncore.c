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

typedef struct Vector Vector;
typedef struct Vector32 Vector32;
typedef struct Vector32u Vector32u;
typedef struct Player PlayerType;

#define FALL_SLOW_DOWN 0.24f
#define FALL_DAMAGE_VELOCITY 0.58f
#define FALL_DAMAGE_SCALAR 4096

#define FOG_DISTANCE 128

/* isvoxelsolid() but water is nonsolid and out of bounds returns true. */
int clip_player(float ox, float oy, float oz, const uint8_t *solidData, int wrap) {
	/* Flooring is important here; default is trunc which is probably hazardous for the world borders. */
	uint32_t x = floorf(ox), y = floorf(oy);
	int32_t z = floorf(oz);

	if (wrap) {
		x %= 512;
		y %= 512;
	} else if (x >= 512 || y >= 512)
		return 1;

	if (z >= 64)
		return 1;

	if (z < 0)
		return 0;

	if (z == 63)
		z = 62;

	return pvx_voxel_get_solidity4(solidData, CALC_I(x, y), z);
}

/* isvoxelsolid() but with wrapping. Seems hazardous. */
int isvoxelsolidwrap(uint32_t x, uint32_t y, int32_t z, const uint8_t *solidData) {
	if (z < 0)
		return 0;

	if (z >= 64)
		return 1;

	return pvx_voxel_get_solidity4(solidData, CALC_I(x & 511, y & 511), z);
}

/* isvoxelsolid() but water is nonsolid? What even is isvoxelsolid(), anyway? */
int clip_grenade(uint32_t x, uint32_t y, int32_t z, const uint8_t *solidData, int correct) {
	if (correct) {
		x %= 512;
		y %= 512;
	} else if (x >= 512 || y >= 512)
		return 0;

	if (z < 0)
		return 0;

	if (z >= 64)
		return 1;

	if (z == 63)
		z = 62;

	return pvx_voxel_get_solidity4(solidData, CALC_I(x, y), z);
}

#if 1
/* Hey, this raycaster actually works properly! */
/* Relevant:
 * https://www.cs.yorku.ca/~amana/research/grid.pdf
 * https://github.com/fenomas/fast-voxel-raycast
 * https://voxel.wiki/wiki/raytracing/
 * https://voxel.wiki/wiki/raycasting/
 */
static int cast2(const uint8_t *solidData, float startX, float startY, float startZ, float endX, float endY,
                float endZ, float length, int32_t *x, int32_t *y, int32_t *z, int last) {
	int stepx, stepy, stepz;
	float offx, offy, offz;
	float deltax, deltay, deltaz;
	float tmaxx, tmaxy, tmaxz;
	float distx, disty, distz;
	float hypot;

	/* Position of endX relative to startX */
	offx = endX - startX;
	offy = endY - startY;
	offz = endZ - startZ;

#if 0
	/* Normalize that relative position. This is only needed if hypot ends up as a really small value
	 * -- multiplying offx,y,z by a really large number would have a similar effect.
	 * The need for this could probably be mitigated with double precision, too.
	 */
	hypot = sqrtf(offx*offx + offy*offy + offz*offz);
	offx /= hypot;
	offy /= hypot;
	offz /= hypot;
#endif

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

		/* NOTE: this should be floored and probably modulo'd (not remaindered) */
		if (isvoxelsolidwrap(voxx, voxy, voxz, solidData)) {
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
#endif

/* Silly VOXLAP function that either rounds or floors depending on your chosen branch of PySnip. */
static int32_t ftol(float f) {
	return f;
}

/* Trash raycaster pyspades uses for grenade explosions. */
static int cast(const uint8_t *solidData, float startX, float startY, float startZ, float endX, float endY,
                float endZ, float length, int32_t *x, int32_t *y, int32_t *z) {
	Vector f, g;
	Vector32 cursor, end, step, p, i;
	uint32_t cnt = 0;

	cursor.x = ftol(startX - .5f);
	cursor.y = ftol(startY - .5f);
	cursor.z = ftol(startZ - .5f);

	end.x = ftol(endX - .5f);
	end.y = ftol(endY - .5f);
	end.z = ftol(endZ - .5f);

	step.x = endX < startX ? -1 : 1;
	step.y = endY < startY ? -1 : 1;
	step.z = endZ < startZ ? -1 : 1;

	f.x = fabsf(startX - cursor.x) + (end.x > cursor.x);
	f.y = fabsf(startY - cursor.y) + (end.y > cursor.y);
	f.z = fabsf(startZ - cursor.z) + (end.z > cursor.z);

	/* The multiplication by 1024 here is to deal with problems caused by too-small floats */
	g.x = fabsf(startX - endX) * 1024;
	g.y = fabsf(startY - endY) * 1024;
	g.z = fabsf(startZ - endZ) * 1024;

	cnt = abs(cursor.x - end.x) + abs(cursor.y - end.y) + abs(cursor.z - end.z);

	if (cursor.x == end.x)
		f.x = g.x = 0;
	if (cursor.y == end.y)
		f.y = g.y = 0;
	if (cursor.z == end.z)
		f.z = g.z = 0;

	p.x = ftol(f.x * g.z - f.z * g.x);
	p.y = ftol(f.y * g.z - f.z * g.y);
	p.z = ftol(f.y * g.x - f.x * g.y);

	i.x = ftol(g.x);
	i.y = ftol(g.y);
	i.z = ftol(g.z);

	if (cnt > length)
		cnt = length;

	while (cnt) {
		/* The use of bitwise OR here scares me. Wonder if it's not supposed to be there at all. (Probably yes, it isn't.) */
		if ((p.x | p.y) >= 0 && cursor.z != end.z) {
			cursor.z += step.z;
			p.x -= i.x;
			p.y -= i.y;
		} else if (p.z >= 0 && cursor.x != end.x) {
			cursor.x += step.x;
			p.x += i.z;
			p.z -= i.y;
		} else { /* Of course this one doesn't bother with end checking. */
			cursor.y += step.y;
			p.y += i.z;
			p.z += i.x;
		}

		if (isvoxelsolidwrap(cursor.x, cursor.y, cursor.z, solidData)) {
			*x = cursor.x;
			*y = cursor.y;
			*z = cursor.z;
			return 1;
		}

		cnt--;
	}

	return 0;
}

int can_see(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1, float z1) {
	int32_t trash;

	return !cast(solidData, x0, y0, z0, x1, y1, z1, FOG_DISTANCE, &trash, &trash, &trash);
}

#if 0
int cast_ray(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1,
			  float z1, float length, int32_t *x, int32_t *y, int32_t *z) {
	x1 = x0 + x1 * length;
	y1 = y0 + y1 * length;
	z1 = z0 + z1 * length;

	return cast(solidData, x0, y0, z0, x1, y1, z1, length, x, y, z);
}
#else
int cast_ray(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1,
			  float z1, float length, int32_t *x, int32_t *y, int32_t *z) {
	x1 = x0 + x1;
	y1 = y0 + y1;
	z1 = z0 + z1;

	return cast2(solidData, x0, y0, z0, x1, y1, z1, length, x, y, z, 0);
}

int cast_ray2(const uint8_t *solidData, fvec3p start, fvec3p end) {
	int32_t trash;
	return cast2(solidData, start.x, start.y, start.z, end.x, end.y, end.z, 0, &trash, &trash, &trash, 0);
}

int cast_ray_last(const uint8_t *solidData, float x0, float y0, float z0, float x1, float y1,
			  float z1, float length, int32_t *x, int32_t *y, int32_t *z) {
	x1 = x0 + x1;
	y1 = y0 + y1;
	z1 = z0 + z1;

	return cast2(solidData, x0, y0, z0, x1, y1, z1, length, x, y, z, 1);
}
#endif

void block_line(int32_t startX, int32_t startY, int32_t startZ, int32_t endX, int32_t endY, int32_t endZ, struct BitmaskUData *map, const uint8_t *color) {
	/* d. . . di. . . diamonds?! */
	Vector32u off, d, di;
	Vector32 cursor, step;
	uint32_t maxoff;

	/* End position's offset relative to the start position. */
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

void try_uncrouch(PlayerType *p, const uint8_t *solidData, int wrap) {
	float x1 = p->position.x + 0.45f;
	float x2 = p->position.x - 0.45f;
	float y1 = p->position.y + 0.45f;
	float y2 = p->position.y - 0.45f;
	float z1 = p->position.z + 2.25f;

	/* First check if player can lower feet if in midair. */
	if (p->airborne && !(clip_player(x1, y1, z1, solidData, wrap) || clip_player(x1, y2, z1, solidData, wrap) || clip_player(x2, y1, z1, solidData, wrap) || clip_player(x2, y2, z1, solidData, wrap)))
		return;

	/* By the time this has been reached, the player cannot lower feet, so the best option is to raise the head instead. */
	p->position.z -= 0.9f;
}

int try_uncrouch_me(PlayerType *p, const uint8_t *solidData, int wrap) {
	float x1 = p->position.x + 0.45f;
	float x2 = p->position.x - 0.45f;
	float y1 = p->position.y + 0.45f;
	float y2 = p->position.y - 0.45f;
	float z1 = p->position.z + 2.25f;
	float z2 = p->position.z - 1.35f;

	/* First check if player can lower feet if in midair. */
	if (p->airborne && !(clip_player(x1, y1, z1, solidData, wrap) || clip_player(x1, y2, z1, solidData, wrap) || clip_player(x2, y1, z1, solidData, wrap) || clip_player(x2, y2, z1, solidData, wrap)))
		return 1;

	/* By the time this has been reached, the player cannot lower feet, so raise the head instead if possible. */
	if (!(clip_player(x1, y1, z2, solidData, wrap) || clip_player(x1, y2, z2, solidData, wrap) || clip_player(x2, y1, z2, solidData, wrap) || clip_player(x2, y2, z2, solidData, wrap))) {
		p->position.z -= 0.9f;
		return 1;
	}

	/* Nothing seemed possible. */
	return 0;
}

/* Don't call this unless crouch state is actually changing. */
void change_crouch(int crouching, PlayerType *p, const uint8_t *solidData, int wrap) {
	if (crouching && !p->airborne)
		p->position.z += 0.9;
	else
		try_uncrouch(p, solidData, wrap);
}

int change_crouch_me(int crouching, PlayerType *p, const uint8_t *solidData, int wrap) {
	if (crouching && !p->airborne) {
		p->position.z += 0.9;
		return 1;
	}
	return try_uncrouch_me(p, solidData, wrap);
}

static int satan(float z, float zgreaterequal, float x1, float y1, float x2, float y2, float ztest, const uint8_t *solidData, int wrap) {
	for (; z >= zgreaterequal && !clip_player(x1, y1, ztest + z, solidData, wrap) && !clip_player(x2, y2, ztest + z, solidData, wrap); z -= 0.9);
	return z < zgreaterequal;
}

/* Like the demon core, but for player movement. */
#define SATANIC_LOOP(zstart, zgreaterequal, x1, y1, x2, y2) satan(zstart, zgreaterequal, x1, y1, x2, y2, nz, solidData, wrap)
static void boxclipmove(PlayerType *p, float secondsSinceLastUpdate, const uint8_t *solidData, int wrap) {
	float f = secondsSinceLastUpdate * 32;
	float nextp, testp;
	float nz;
	int canclimb = !(p->inputs & (KeyStateTypeCrouch | KeyStateTypeSprint)) && p->orientation.z < 0.5;
	int climb = 0;

	float offset, m;
	if (p->inputs & KeyStateTypeCrouch) {
		offset = 0.45;
		m = 0.9;
	} else {
		offset = 0.9;
		m = 1.35;
	}

	nz = p->position.z + offset;

	nextp = f * p->velocity.x + p->position.x;
	testp = nextp + (p->velocity.x < 0 ? -0.45 : 0.45);
	if (SATANIC_LOOP(m, -1.36, testp, p->position.y - 0.45, testp, p->position.y + 0.45))
		p->position.x = nextp;
	else if (canclimb && SATANIC_LOOP(0.35, -2.36, testp, p->position.y - 0.45, testp, p->position.y + 0.45)) {
		p->position.x = nextp;
		climb = 1;
	} else
		p->velocity.x = 0;

	nextp = f * p->velocity.y + p->position.y;
	testp = nextp + (p->velocity.y < 0 ? -0.45 : 0.45);
	if (SATANIC_LOOP(m, -1.36, p->position.x - 0.45, testp, p->position.x + 0.45, testp))
		p->position.y = nextp;
	else if (!climb) {
		if (canclimb && SATANIC_LOOP(0.35, -2.36, p->position.x - 0.45, testp, p->position.x + 0.45, testp)) {
			p->position.y = nextp;
			climb = 1;
		} else
			p->velocity.y = 0;
	}

	if (climb) {
		p->velocity.x *= 0.5;
		p->velocity.y *= 0.5;
		nz--;
		m = -1.35;
	} else {
		if (p->velocity.z < 0)
			m = -m;

		/* Since multiplication is evaluated left-to-right, I'm not sure if replacing this with f * p->velocity.z would be safe. */
		nz += p->velocity.z * secondsSinceLastUpdate * 32;
	}

#define SCLIP(xoff, yoff) clip_player(p->position.x + (xoff), p->position.y + (yoff), nz + m, solidData, wrap)
	/* secondsSinceLastUpdate not being 1/60 can screw with some calculations here (see nz). Makes players nice and jittery. Wonder if I can (should) force it, somehow, to 1/60 for just the Z axis? */
	p->airborne = 1;
	if (SCLIP(-0.45, -0.45) || SCLIP (-0.45, 0.45) || SCLIP(0.45, -0.45) || SCLIP(0.45, 0.45)) {
		if (p->velocity.z >= 0) {
			p->wade = p->position.z > 61;
			p->airborne = 0;
		}

		p->velocity.z = 0;
	} else
		p->position.z = nz - offset;
}

static float calc_acceleration(PlayerType *p, float secondsSinceLastUpdate, int diagonal) {
	float acceleration = secondsSinceLastUpdate;

	/* Affect horizontal movement acceleration based on airborne/inputs. */
	if (p->airborne)
		acceleration *= 0.1;
	else if (p->inputs & KeyStateTypeCrouch)
		acceleration *= 0.3;
	else if ((p->mouseInputs & WeaponInputTypeSecondary && p->item == ToolTypeGun) || p->inputs & KeyStateTypeSneak)
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
int32_t move_player(PlayerType *p, float secondsSinceLastUpdate, const uint8_t *solidData, int wrap) {
	float horizontalHypotenuse, acceleration, friction, oldZVelocity, verticalMove, horizontalMove;
	Vector rightSide;

	/* Get the player's right side, but in 2D instead of 3D.
	 * Trivia time: notice how this is 2D, not 3D.
	 * This is why looking down and moving left/right is full speed, but moving forward/backward is not.*/
	horizontalHypotenuse = sqrtf(p->orientation.x * p->orientation.x + p->orientation.y * p->orientation.y);
	if (horizontalHypotenuse == 0)
		horizontalHypotenuse = 1;
	rightSide.x = -p->orientation.y / horizontalHypotenuse;
	rightSide.y = p->orientation.x / horizontalHypotenuse;

	/* Vertical/horizontal in a 2D sense. Most other instances of vertical refer to the axis
	 * which gravity operates along, whereas horizontal refers to the other two axes. */
	verticalMove = (p->inputs & KeyStateTypeForward) ? 1 : ((p->inputs & KeyStateTypeBackward) ? -1 : 0);
	horizontalMove = (p->inputs & KeyStateTypeLeft) ? -1 : ((p->inputs & KeyStateTypeRight) ? 1 : 0);
	acceleration = calc_acceleration(p, secondsSinceLastUpdate, verticalMove && horizontalMove);

	if ((p->inputs & KeyStateTypeJump)) {
		p->inputs &= ~KeyStateTypeJump;
		p->velocity.z = -0.36;
	}

	/* Add to velocity in the direction player is trying to move. Do vertical air friction too. */
	p->velocity.x += p->orientation.x * acceleration * verticalMove;
	p->velocity.y += p->orientation.y * acceleration * verticalMove;
	p->velocity.x += rightSide.x * acceleration * horizontalMove;
	p->velocity.y += rightSide.y * acceleration * horizontalMove;
	p->velocity.z += secondsSinceLastUpdate;
	/* All of this adding 1 seems like a fire hazard. */
	p->velocity.z /= secondsSinceLastUpdate + 1;

	/* Horizontal friction, for water, air or ground. */
	friction = secondsSinceLastUpdate * (p->wade ? 6 : (p->airborne ? 1 : 4)) + 1;
	p->velocity.x /= friction;
	p->velocity.y /= friction;

	/* Call into the demon core. */
	oldZVelocity = p->velocity.z;
	boxclipmove(p, secondsSinceLastUpdate, solidData, wrap);

	if (wrap) {
		p->position.x = fmodf(p->position.x, 512);
		p->position.y = fmodf(p->position.y, 512);
		if (p->position.x < 0)
			p->position.x += 512;
		if (p->position.y < 0)
			p->position.y += 512;
	}

	if (p->velocity.z == 0 && (oldZVelocity > FALL_SLOW_DOWN)) {
		/* Player went from an airborne state to a grounded one, and had enough velocity to slow down. */
		p->velocity.x *= 0.5;
		p->velocity.y *= 0.5;

		if (oldZVelocity > FALL_DAMAGE_VELOCITY) {
			/* Player also had enough velocity to be damaged. */
			oldZVelocity -= FALL_DAMAGE_VELOCITY;
			return oldZVelocity * oldZVelocity * FALL_DAMAGE_SCALAR;
		}

		return -1; /* No fall damage but play inaudible fall sound. */
	}

	return 0; /* No fall damage. */
}

/* Returns 1 if the grenade collides with something, otherwise 0. */
int move_grenade(struct Grenade *g, float secondsSinceLastUpdate, const uint8_t *solidData, int correct) {
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

	if (!clip_grenade(newPos.x, newPos.y, newPos.z, solidData, correct))
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
		if      (newPos.z != oldPos.z && ((newPos.x == oldPos.x && newPos.y == oldPos.y) || !clip_grenade(newPos.x, newPos.y, oldPos.z, solidData, correct)))
			g->vel.z = -g->vel.z;
		else if (newPos.x != oldPos.x && ((newPos.y == oldPos.y && newPos.z == oldPos.z) || !clip_grenade(oldPos.x, newPos.y, newPos.z, solidData, correct)))
			g->vel.x = -g->vel.x;
		else if (newPos.y != oldPos.y && ((newPos.x == oldPos.x && newPos.z == oldPos.z) || !clip_grenade(newPos.x, oldPos.y, newPos.z, solidData, correct)))
			g->vel.y = -g->vel.y;

		return 1;
	}
}
