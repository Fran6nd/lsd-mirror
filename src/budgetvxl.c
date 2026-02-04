/* See bottom of file for license. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

struct BitmaskUData {
	uint8_t *solidData;
	uint8_t *colorData;
};

struct PositionSet {
	uint_fast32_t pos[3];
	uint_fast32_t size[3];
};

struct ColumnIGroup {
	size_t i[5];
};

struct SpanHeader {
	uint8_t length;
	uint8_t topColorStart;
	uint8_t topColorEnd;
	uint8_t airStart;
};

static int pvx_voxel_get_solidity4(const uint8_t *outBuf,
                                   const size_t i,
                                   const uint_fast32_t z) {
	size_t i2 = z + i;
	return !!(outBuf[i2 >> 3] & (1 << (i2 & 7)));
}

static size_t pvx_calc_solidity_i(uint_fast32_t x, uint_fast32_t y, size_t xSize, size_t zSize) {
	return x * zSize + y * xSize * zSize;
}

static void pvx_calc_solidity_i_group(struct ColumnIGroup *out, uint_fast32_t x, uint_fast32_t y, size_t xSize, size_t zSize) {
	out->i[0] = pvx_calc_solidity_i(x, y, xSize, zSize);
	out->i[1] = pvx_calc_solidity_i(x-1, y, xSize, zSize);
	out->i[2] = pvx_calc_solidity_i(x+1, y, xSize, zSize);
	out->i[3] = pvx_calc_solidity_i(x, y-1, xSize, zSize);
	out->i[4] = pvx_calc_solidity_i(x, y+1, xSize, zSize);
}

static int check_surface_voxel(const uint8_t *outBuf,
                               struct ColumnIGroup *i,
                               struct PositionSet *set) {
#define x set->pos[0]
#define y set->pos[1]
#define z set->pos[2]
#define xSize set->size[0]
#define ySize set->size[1]
#define zSize set->size[2]
	if (z == 0) {
		return 1;
	}

	if (x > 0) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[1], z))
			return 1;
	} if (x < xSize - 1) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[2], z))
			return 1;
	} if (y > 0) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[3], z))
			return 1;
	} if (y < ySize - 1) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[4], z))
			return 1;
	} if (z > 0) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[0], z-1))
			return 1;
	} if (z < zSize - 1) {
		if (!pvx_voxel_get_solidity4(outBuf, i->i[0], z+1))
			return 1;
	}

	return 0;
#undef x
#undef y
#undef z
#undef xSize
#undef ySize
#undef zSize
}

#include <stdio.h>
size_t pvx_dump_vxl(struct BitmaskUData *mapData, uint_fast32_t x, uint_fast32_t y, uint_fast32_t xSize, uint_fast32_t ySize, uint_fast32_t zSize, uint8_t *outMapData, size_t colCount) {
	struct SpanHeader *header = (void *)outMapData;
	struct ColumnIGroup i;
	struct PositionSet set;
	uint8_t length;
	size_t curColCount = 0;

	set.size[0] = xSize;
	set.size[1] = ySize;
	set.size[2] = zSize;

	set.pos[0] = x;
	set.pos[1] = y;

#define x set.pos[0]
#define y set.pos[1]
#define z set.pos[2]

	goto loop;
	for (y=0;y<ySize;y++) {
		for (x=0;x<xSize;x++) {
loop:
			/* TODO: instead of recalc all, just use e.g. + 64, - 64; . . . */
			pvx_calc_solidity_i_group(&i, x, y, xSize, zSize);
			z = 0;

			while (1) {
				header->airStart = z;
				length = 1;

				/* That whole if statement is there for when the water is not solid. pyspades allows it for some reason so we do too. */
				while (!pvx_voxel_get_solidity4(mapData->solidData, i.i[0], z)) if (++z == zSize) {header->length = 0; header->topColorStart = 64; header->topColorEnd = 63; header++; goto breakloop;};
				header->topColorStart = z;

				while (z < zSize && check_surface_voxel(mapData->solidData, &i, &set) && pvx_voxel_get_solidity4(mapData->solidData, i.i[0], z)) {
					memcpy(header+length, mapData->colorData+(z+i.i[0])*3, 3);
					((uint8_t *)(header+length))[3] = 0;
					length++;
					z++;
				}

				header->topColorEnd = z-1;
				if (z == zSize) {header->length = 0; header += length; break;}

				while (!check_surface_voxel(mapData->solidData, &i, &set) && pvx_voxel_get_solidity4(mapData->solidData, i.i[0], z)) {
					if (++z == zSize) {header->length = 0; header += length; goto breakloop;}
				}

				while (check_surface_voxel(mapData->solidData, &i, &set) && pvx_voxel_get_solidity4(mapData->solidData, i.i[0], z)) {
					memcpy(header+length, mapData->colorData+(z+i.i[0])*3, 3);
					((uint8_t *)(header+length))[3] = 0;
					length++;

					/* Prevent nonsolid water from destroying everything. */
					if (++z == zSize) {header->length = length; header += length; header->airStart = 63; header->length = 0; header->topColorStart = 64; header->topColorEnd = 63; header++; goto breakloop;}
				}

				header->length = length;
				header += length;
			}
			breakloop:
			curColCount++;
			if (curColCount == colCount) return (uint8_t *)header-outMapData;
		}
	}

	return (uint8_t *)header-outMapData;

#undef x
#undef y
#undef z
}

/*
 * Copyright (c) 2024-2025 totally not a burner.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
