#ifndef hAgzKrlgjQbjfaIKqzBEvpoqbHqexOVl
#define hAgzKrlgjQbjfaIKqzBEvpoqbHqexOVl

#include <stdint.h>
#include "state.h" /* fvec3, ivec3 */


static inline ivec3 vtoi(fvec3 v1)
{
	ivec3 v = {floorf(v1.x), floorf(v1.y), floorf(v1.z)};
	return v;
}
static inline fvec3 itov(ivec3 v1)
{
	fvec3 v = {v1.x, v1.y, v1.z};
	return v;
}


static inline fvec3 vadd(fvec3 v1, float val)
{
	fvec3 v = {v1.x + val, v1.y + val, v1.z + val};
	return v;
}
static inline ivec3 iadd(ivec3 v1, int32_t val)
{
	ivec3 v = {v1.x + val, v1.y + val, v1.z + val};
	return v;
}


static inline fvec3 vaddv(fvec3 v1, fvec3 v2)
{
	fvec3 v = {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
	return v;
}
static inline ivec3 iaddi(ivec3 v1, ivec3 v2)
{
	ivec3 v = {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
	return v;
}


static inline fvec3 vsubv(fvec3 v1, fvec3 v2)
{
	fvec3 v = {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
	return v;
}
static inline ivec3 isubi(ivec3 v1, ivec3 v2)
{
	ivec3 v = {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
	return v;
}


static inline fvec3 vmul(fvec3 v1, float val)
{
	fvec3 v = {v1.x * val, v1.y * val, v1.z * val};
	return v;
}
static inline ivec3 imul(ivec3 v1, int32_t val)
{
	ivec3 v = {v1.x * val, v1.y * val, v1.z * val};
	return v;
}


static inline fvec3 divv(float val, fvec3 v2)
{
	fvec3 v = {val / v2.x, val / v2.y, val / v2.z};
	return v;
}
static inline ivec3 divi(float val, ivec3 v2)
{
	ivec3 v = {val / v2.x, val / v2.y, val / v2.z};
	return v;
}


static inline fvec3 fdiv(fvec3 v1, float val)
{
	fvec3 v = {v1.x / val, v1.y / val, v1.z / val};
	return v;
}
static inline ivec3 idiv(ivec3 v1, float val)
{
	ivec3 v = {v1.x / val, v1.y / val, v1.z / val};
	return v;
}


static inline fvec3 vabs(fvec3 v1)
{
	fvec3 v = {fabsf(v1.x), fabsf(v1.y), fabsf(v1.z)};
	return v;
}
static inline ivec3 iabs(ivec3 v1)
{
	ivec3 v = {abs(v1.x), abs(v1.y), abs(v1.z)};
	return v;
}


static inline fvec3 vfloor(fvec3 v1)
{
	fvec3 v = {floorf(v1.x), floorf(v1.y), floorf(v1.z)};
	return v;
}


static inline fvec3 vceil(fvec3 v1)
{
	fvec3 v = {ceilf(v1.x), ceilf(v1.y), ceilf(v1.z)};
	return v;
}


static inline fvec3 vsign(fvec3 v1)
{
	fvec3 v = {
		copysignf(1, v1.x),
		copysignf(1, v1.y),
		copysignf(1, v1.z)
	};
	return v;
}
static inline ivec3 isign(ivec3 v1)
{
	ivec3 v = {
		v1.x < 0 ? -1 : 1,
		v1.y < 0 ? -1 : 1,
		v1.z < 0 ? -1 : 1
	};
	return v;
}


static inline ivec3 vsignbit(fvec3 v1)
{
	ivec3 v = {
		signbit(v1.x),
		signbit(v1.y),
		signbit(v1.z)
	};
	return v;
}
static inline ivec3 isignbit(ivec3 v1)
{
	ivec3 v = {
		v1.x < 0,
		v1.y < 0,
		v1.z < 0
	};
	return v;
}


static inline fvec3 vchoose(fvec3 v1, fvec3 v2, ivec3 choice)
{
	fvec3 v = {
		choice.x ? v2.x : v1.x,
		choice.y ? v2.y : v1.y,
		choice.z ? v2.z : v1.z
	};
	return v;
}
static inline ivec3 ichoose(ivec3 v1, ivec3 v2, ivec3 choice)
{
	ivec3 v = {
		choice.x ? v2.x : v1.x,
		choice.y ? v2.y : v1.y,
		choice.z ? v2.z : v1.z
	};
	return v;
}


static inline fvec3 vnan_inf(fvec3 v1)
{
	fvec3 v = {
		v1.x != v1.x ? HUGE_VAL : v1.x,
		v1.y != v1.y ? HUGE_VAL : v1.y,
		v1.z != v1.z ? HUGE_VAL : v1.z
	};
	return v;
}


static inline fvec3 v2mod(fvec3 v, float val)
{
	v.x = fmodf(v.x, val);
	v.y = fmodf(v.y, val);

	if (v.x < 0)
		v.x += val;
	if (v.y < 0)
		v.y += val;

	return v;
}


static inline float vlength(fvec3 v)
{
	return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}


static inline float vsum(fvec3 v1)
{
	return v1.x + v1.y + v1.z;
}
static inline int32_t isum(ivec3 v1)
{
	return v1.x + v1.y + v1.z;
}


static inline float vmax(fvec3 v1)
{
	if (v1.x < v1.y && v1.x < v1.z) return v1.x;
	if (v1.y < v1.z)                return v1.y;
	return v1.z;
}
static inline int32_t imax(ivec3 v1)
{
	if (v1.x > v1.y && v1.x > v1.z) return v1.x;
	if (v1.y > v1.z)                return v1.y;
	return v1.z;
}

#endif
