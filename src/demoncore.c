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

#include "veclib.h"

struct Vector32u {uint32_t x, y, z;};
typedef struct Vector32u Vector32u;

#define FALL_SLOW_DOWN 0.24f
#define FALL_DAMAGE_VELOCITY 0.58f
#define FALL_DAMAGE_SCALAR 4096

static float fmodulof(float x, float y)
{
	x = fmodf(x, y);
	if (x < 0)
		return x + y;

	return x;
}

static int phys_solid(int32_t x, int32_t y, int32_t z, Map map)
{
	if (z < 0)
		return 0;
	if (z >= MAP_Z)
		return 1;

	return map_is_solid(map, x, y, z);
}

int clip_phys(float x, float y, float z, Map map, int wrap)
{
	x = floorf(x);
	y = floorf(y);
	z = floorf(z);

	if (wrap) {
		x = fmodulof(x, MAP_X);
		y = fmodulof(y, MAP_Y);
	}

	if (
		x < 0 || x >= MAP_X ||
		y < 0 || y >= MAP_Y
	)
		return 1;

	if (z == MAP_Z - 1)
		z = MAP_Z - 2;

	return phys_solid(x, y, z, map);
}

/* See https://www.eecs.yorku.ca/~amana/research/grid.pdf;
 * https://lodev.org/cgtutor/raycasting.html is also useful.
 */
struct Cast cast_start(fvec3 start, fvec3 end)
{
	fvec3 off   = vsubv(end, start);
	fvec3 delta = vabs(divv(1, off));

	fvec3 lower  = vsubv(vfloor(start), start);
	fvec3 higher = vsubv(vceil (start), start);
	fvec3 side_dist = vnan_inf(vchoose(higher, lower, vsignbit(off)));

	ivec3 vox  = vtoi(start);
	ivec3 step = vtoi(vsign(off));

	int remaining = 1 + vsum(vabs(vsubv(vfloor(end), vfloor(start))));

	struct Cast cast = {remaining, vox, step, side_dist, delta};
	return cast;
}

int cast_step(struct Cast *cast)
{
	int   *remaining = &cast->remaining;
	ivec3 *vox       = &cast->vox;
	ivec3  step      =  cast->step;
	fvec3 *side_dist = &cast->side_dist;
	fvec3  delta     =  cast->delta;

	if (side_dist->x < side_dist->y &&
	    side_dist->x < side_dist->z) {
		      vox->x +=  step.x;
		side_dist->x += delta.x;
	} else if (side_dist->y < side_dist->z) {
		      vox->y +=  step.y;
		side_dist->y += delta.y;
	} else {
		      vox->z +=  step.z;
		side_dist->z += delta.z;
	}

	return --(*remaining) > 0;
}

int cast_ray(Map map, fvec3 start, fvec3 end, ivec3 *vox)
{
	struct Cast cast = cast_start(start, end);

	do {
		ivec3 vx = cast.vox;

		if (vox != NULL)
			*vox = vx;

		if (phys_solid(vx.x & 511, vx.y & 511, vx.z, map))
			return 1;
	} while (cast_step(&cast));

	return 0;
}

int cast_ray_last(Map map, fvec3 start, fvec3 end, ivec3 *vox)
{
	struct Cast cast = cast_start(start, end);

	if (vox != NULL)
		*vox = cast.vox;

	do {
		ivec3 vx = cast.vox;

		if (phys_solid(vx.x & 511, vx.y & 511, vx.z, map))
			return 1;

		if (vox != NULL)
			*vox = vx;
	} while (cast_step(&cast));

	return 0;
}

#define HUGE_INT32 2147483647L
struct BlockLine block_line_start(ivec3 start, ivec3 end)
{
	ivec3 off = isubi(end, start);
	ivec3 aoff = iabs(off);

	/* otherwise-unnecessary variable needed for AoS compatibility */
	int32_t maxoff = imax(aoff) * 1024;

	ivec3 delta = {
		aoff.x == 0 ? HUGE_INT32 : maxoff / aoff.x,
		aoff.y == 0 ? HUGE_INT32 : maxoff / aoff.y,
		aoff.z == 0 ? HUGE_INT32 : maxoff / aoff.z
	};
	ivec3 side_dist = idiv(delta, 2);

	ivec3 vox = start;
	ivec3 step = isign(off);

	ivec3 positive = iadd(step, 1);
	side_dist = ichoose(side_dist, isubi(delta, side_dist), positive);

	int remaining = 1 + isum(iabs(isubi(end, start)));

	struct BlockLine line = {remaining, vox, step, side_dist, delta};
	return line;
}

int block_line_step(struct BlockLine *line)
{
	int   *remaining = &line->remaining;
	ivec3 *vox       = &line->vox;
	ivec3  step      =  line->step;
	ivec3 *side_dist = &line->side_dist;
	ivec3  delta     =  line->delta;

	if (side_dist->z <= side_dist->x &&
	    side_dist->z <= side_dist->y) {
		      vox->z +=  step.z;
		side_dist->z += delta.z;
	} else if (side_dist->x < side_dist->y) {
		      vox->x +=  step.x;
		side_dist->x += delta.x;
	} else {
		      vox->y +=  step.y;
		side_dist->y += delta.y;
	}

	return --(*remaining) > 0;
}

void block_line(Map map, ivec3 start, ivec3 end, color color)
{
	struct BlockLine line = block_line_start(start, end);

	do {
		ivec3 vox = line.vox;
		size_t xy = map_to_xy(vox.x, vox.y);

		if (!map_is_solid_xy(map, xy, vox.z)) {
			map_create_xy(map, xy, vox.z);
			map_set_color_xy(map, xy, vox.z, color, 1);
		}
	} while (block_line_step(&line));
}

#define clip_plane_point(xoff, yoff) \
	clip_phys(p->pos.x + (xoff), p->pos.y + (yoff), nz + m, map, wrap)

#define clip_plane() ( \
	clip_plane_point(-0.45, -0.45) || clip_plane_point(-0.45, 0.45) || \
	clip_plane_point( 0.45, -0.45) || clip_plane_point( 0.45, 0.45)    \
)

static void try_uncrouch(struct Player *p, Map map, int wrap)
{
	float nz = p->pos.z + 2.25f;
	float m  = 0;

	/* Try lowering feet */
	if (p->airborne && clip_plane())
		return;

	/* Raise head */
	p->pos.z -= 0.9f;
}

static int try_uncrouch_me(struct Player *p, Map map, int wrap)
{
	float nz = p->pos.z + 2.25f;
	float m  = 0;

	/* Try lowering feet */
	if (p->airborne && clip_plane())
		return 1;

	/* Try raising head */
	nz = p->pos.z - 1.35f;

	/* TODO: pass nz/m directly to clip_plane */
	if (!clip_plane()) {
		p->pos.z -= 0.9f;
		return 1;
	}

	/* Nothing was possible */
	return 0;
}

/* Don't call this unless crouch state is actually changing. */
void change_crouch(int crouching, struct Player *p, Map map, int wrap)
{
	if (crouching && !p->airborne)
		p->pos.z += 0.9;
	else
		try_uncrouch(p, map, wrap);
}

int change_crouch_me(int crouching, struct Player *p, Map map, int wrap)
{
	if (crouching && !p->airborne) {
		p->pos.z += 0.9;
		return 1;
	}

	return try_uncrouch_me(p, map, wrap);
}

static int check_collision(
	float z, float zmax,
	fvec3 p1, fvec3 p2, float ztest, Map map, int wrap)
{
	for (;
		z >= zmax &&
		!clip_phys(p1.x, p1.y, ztest + z, map, wrap) &&
		!clip_phys(p2.x, p2.y, ztest + z, map, wrap);
			z -= 0.9
	);

	return z < zmax;
}

static void move_or_collide_on_axis(
	float nextp, float *apos, float *avel, int *climb, int canclimb,
	float m, float nz, Map map, fvec3 p1, fvec3 p2, int wrap)
{
	if (check_collision(m, -1.36, p1, p2, nz, map, wrap)) {
		*apos = nextp;
		return;
	}

	if (!(*climb)) {
		if (canclimb &&
			check_collision(0.35, -2.36, p1, p2, nz, map, wrap)
		) {
			*apos = nextp;
			*climb = 1;
		} else
			*avel = 0;
	}
}

typedef union {
	fvec3 v;
	float f[3];
} fvec3u;

static void move_axis(
	int axis, fvec3 *pos, fvec3 *vel, int *climb, int canclimb, float m,
	float nz, Map map, int wrap, float time_delta)
{
	fvec3u p1, p2;
	fvec3u *posu = (fvec3u *)pos;
	fvec3u *velu = (fvec3u *)vel;

	fvec3u nextp = {vaddv(vmul(*vel, time_delta * 32), *pos)};
	fvec3u testp = {{
		nextp.v.x + copysignf(0.45, (*vel).x),
		nextp.v.y + copysignf(0.45, (*vel).y),
		0
	}};

	p1.v = vadd(*pos, -0.45);
	p2.v = vadd(*pos,  0.45);

	p1.f[axis] = testp.f[axis];
	p2.f[axis] = testp.f[axis];

	move_or_collide_on_axis(
		nextp.f[axis],
		&posu->f[axis],
		&velu->f[axis],
		climb,
		canclimb,
		m,
		nz,
		map,
		p1.v,
		p2.v,
		wrap
	);
}

static void boxclipmove(struct Player *p, float time_delta, Map map, int wrap)
{
	float nz;
	int canclimb =
		!(p->inputs & (KeyStateTypeCrouch | KeyStateTypeSprint)) &&
		p->ori.z < 0.5;
	int climb = 0;

	float offset, m;

	m = p->inputs & KeyStateTypeCrouch ?
		0.9 : 1.35;
	offset = m - 0.45;

	nz = p->pos.z + offset;

	move_axis(0, &p->pos, &p->vel,
		&climb, canclimb, m, nz, map, wrap, time_delta);
	move_axis(1, &p->pos, &p->vel,
		&climb, canclimb, m, nz, map, wrap, time_delta);

	if (climb) {
		p->vel.x /= 2;
		p->vel.y /= 2;
		nz--;
		m = -1.35;
	} else {
		nz += p->vel.z * time_delta * 32;
		m = copysignf(m, p->vel.z);
	}

	p->airborne = 1;

	if (clip_plane()) {
		if (p->vel.z >= 0) {
			p->wade = p->pos.z > 61;
			p->airborne = 0;
		}

		p->vel.z = 0;
	} else
		p->pos.z = nz - offset;
}

static float calc_acceleration(struct Player *p, float time_delta, int diagonal)
{
	float accel = time_delta;

	int move  = p->inputs;
	int mouse = p->mouseInputs;
	int holding_gun = p->tool == ToolTypeGun;

	if (p->airborne)
		accel *= 0.1;
	else if (move & KeyStateTypeCrouch)
		accel *= 0.3;
	else if (mouse & GunInputTypeSecondary && holding_gun)
		accel *= 0.5;
	else if (move & KeyStateTypeSneak)
		accel *= 0.5;
	else if (move & KeyStateTypeSprint)
		accel *= 1.3;

	if (diagonal)
		accel *= 0.70710678f;

	return accel;
}

static float calc_friction(struct Player *p, float time_delta)
{
	if (p->wade)
		return 1 + time_delta * 6;
	if (p->airborne)
		return 1 + time_delta * 1;

	return 1 + time_delta * 4;
}

int32_t move_player(struct Player *p, float time_delta, Map map, int wrap)
{
	float horiz_len, accel, friction, oldZVelocity, move_vert, move_horz;
	fvec3 front, right;


	front = p->ori;
	front.z = 0;

	horiz_len = vlength(front);
	if (horiz_len == 0)
		horiz_len = 1;

	right.x = -front.y / horiz_len;
	right.y =  front.x / horiz_len;
	right.z = 0;


	move_vert = (p->inputs & KeyStateTypeForward)  ? 1 :
	           ((p->inputs & KeyStateTypeBackward) ? -1 : 0);
	move_horz = (p->inputs & KeyStateTypeLeft)     ? -1 :
	           ((p->inputs & KeyStateTypeRight)    ? 1 : 0);


	accel = calc_acceleration(p, time_delta, move_vert && move_horz);

	if (p->inputs & KeyStateTypeJump) {
		p->inputs &= ~KeyStateTypeJump;
		p->vel.z = -0.36;
	}


	p->vel = vaddv(p->vel, vmul(front, accel * move_vert));
	p->vel = vaddv(p->vel, vmul(right, accel * move_horz));

	p->vel.z += time_delta;
	p->vel.z /= time_delta + 1;


	friction = calc_friction(p, time_delta);
	p->vel.x /= friction;
	p->vel.y /= friction;


	oldZVelocity = p->vel.z;
	boxclipmove(p, time_delta, map, wrap);

	if (wrap) /* assumes MAP_X == MAP_Y */
		p->pos = v2mod(p->pos, MAP_X);

	if (!(p->vel.z == 0 && (oldZVelocity > FALL_SLOW_DOWN)))
		return -1;

	p->vel.x /= 2;
	p->vel.y /= 2;

	oldZVelocity -= FALL_DAMAGE_VELOCITY;

	if (oldZVelocity <= 0)
		return 0;

	/* Wouldn't want an integer to overflow. */
	if (oldZVelocity >= 724.0774)
		return 2147483647;

	return oldZVelocity * oldZVelocity * FALL_DAMAGE_SCALAR;
}

#define only_axis_differs(a, b, c, altcond) ( \
	v1.a != v2.a && ((v1.b == v2.b && v1.c == v2.c) || altcond) \
)

/* Returns 1 if the grenade collides with something. */
int move_grenade(struct Grenade *g, float time_delta, Map map)
{
	ivec3 v1, v2;
	fvec3 pos = g->pos;

	g->vel.z += time_delta;

	v1 = vtoi(pos);
	pos = vaddv(pos, vmul(g->vel, time_delta * 32));
	v2 = vtoi(pos);

	if (!clip_phys(v2.x, v2.y, v2.z, map, 1)) {
		g->pos = pos;
		return 0;
	}

	pos = g->pos;
	g->vel = vmul(g->vel, 0.36);

	if (only_axis_differs(z, x, y, !clip_phys(v2.x, v2.y, v1.z, map, 1))) {
		g->vel.z = -g->vel.z;
		return 1;
	}

	if (only_axis_differs(x, y, z, !clip_phys(v2.x, v2.y, v1.z, map, 1))) {
		g->vel.x = -g->vel.x;
		return 1;
	}

	if (only_axis_differs(y, x, z, !clip_phys(v2.x, v2.y, v1.z, map, 1))) {
		g->vel.y = -g->vel.y;
		return 1;
	}

	return 1;
}
