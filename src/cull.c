/* See bottom of file for license. */
#include <stdint.h>
#include <stdlib.h>
#include "bitmask.h"

typedef uint32_t Column;
struct ColumnStack {
	size_t occupiedSize;
	size_t startIndex;
	size_t index;
	Column *data;
};

#define COLUMN_GET_X(column) ((column) & 511)
#define COLUMN_GET_Y(column) (((column) >> 9) & 511)
#define COLUMN_GET_TOP(column) (((column) >> 18) & 63)
#define COLUMN_GET_BOTTOM(column) ((column) >> 24)
#define COLUMN_SET(x, y, top, bottom, column) column = (x) | ((y) << 9) | ((top) << 18) | ((bottom) << 24)

#define MAP_SIZE_X 512
#define MAP_SIZE_Y 512
#define MAP_SIZE_Z 64

static int libspades_voxel_bounds_check_player(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z) {
	return (x < MAP_SIZE_X && y < MAP_SIZE_Y && z < (MAP_SIZE_Z - 2));
}

/** Returns 1 if the given voxel is solid, in-bounds and breakable by a player. Returns 0 otherwise. */
static int libspades_voxel_get_player_breakability(uint_fast32_t x,
                                                   uint_fast32_t y,
                                                   uint_fast32_t z,
                                                   const uint8_t *solidData) {
	return libspades_voxel_bounds_check_player(x, y, z) && pvx_voxel_get_solidity4(solidData, CALC_I(x, y), z);
}

Column detect_column(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const uint8_t *solidData) {
	Column column;
	int_fast8_t top, bottom;
	size_t i = CALC_I(x, y);

	if (!libspades_voxel_bounds_check_player(x, y, z) || !pvx_voxel_get_solidity4(solidData, i, z))
		return 0;

	/* Find the top. */
	top = z - 1;
	while (top >= 0) {
		if (!pvx_voxel_get_solidity4(solidData, i, top))
			break;
		top--;
	}

	/* Find the bottom. */
	bottom = z + 1;
	while (bottom <= 62) {
		if (!pvx_voxel_get_solidity4(solidData, i, bottom))
			break;
		bottom++;
	}

	if (bottom == 63)
		return -1;

	COLUMN_SET(x, y, top + 1, bottom - 1, column);

	return column;
}

static int detect_and_push_columns(int_fast16_t x,
                                   int_fast16_t y,
                                   int_fast8_t top,
                                   int_fast8_t bottom,
                                   struct ColumnStack *stack,
                                   uint64_t (*rememberedSolidity)[512],
                                   uint64_t (*keepSolid)[512],
                                   const uint8_t *solidData) {
	Column detectedColumn;
	int_fast8_t z, detectedTop;
	size_t i = CALC_I(x, y);

	z = top;

	/* TODO: Start at the bottom; stack is a misnomer. */
	/* Properly handle columns which have a top greater than the main column's top. */
	if (pvx_voxel_get_solidity4(solidData, i, z)) {
		/* Detect the column's top. */
		while (z >= 0 && pvx_voxel_get_solidity4(solidData, i, z))
			z--;

		detectedTop = z + 1;

		/* Detect the column's bottom. */
		z = top + 1;

		/* Look for a voxel which is empty or is at the bottom of the map. */
		while (z < (MAP_SIZE_Z - 1) && pvx_voxel_get_solidity4(solidData, i, z))
			z++;

		z--;

		if (z == (MAP_SIZE_Z - 2))
			return 1;

		if (!(rememberedSolidity[x][y] & ((uint64_t)1 << (uint64_t)z))) {
			if (keepSolid[x][y] & ((uint64_t)1 << (uint64_t)z))
				return 1;
			rememberedSolidity[x][y] |= (uint64_t)1 << (uint64_t)z;
			keepSolid[x][y] |= (uint64_t)1 << (uint64_t)z;

			COLUMN_SET(x, y, detectedTop, z, detectedColumn);
			stack->data[stack->occupiedSize++] = detectedColumn;
		}

		z++;
	}

	while (z <= bottom) { /* Greatest entrance Z: 62. Minumum entrance Z: 1. Real max: 55. */
		/* Skip over any non-solid voxels and find the top of a solid voxel column, recording its position. */
		while (!pvx_voxel_get_solidity4(solidData, i, z)) /* Infinite loop here; z too high.  */ /* Likely verdict: completely empty column. */
			z++;

		if (z > bottom)
			break;

		detectedTop = z;

		/* Go to the end of the solid voxel column. */
		while (z < (MAP_SIZE_Z - 1) && pvx_voxel_get_solidity4(solidData, i, z))
			z++;

		z--;

		if (z == (MAP_SIZE_Z - 2))
			return 1;

		/* If the column has not been remembered, remember it and add it to the stack. */
		if (!(rememberedSolidity[x][y] & ((uint64_t)1 << (uint64_t)z))) {
			if (keepSolid[x][y] & ((uint64_t)1 << (uint64_t)z))
				return 1;
			rememberedSolidity[x][y] |= (uint64_t)1 << (uint64_t)z;
			keepSolid[x][y] |= (uint64_t)1 << (uint64_t)z;

			COLUMN_SET(x, y, detectedTop, z, detectedColumn);
			stack->data[stack->occupiedSize++] = detectedColumn;
		}

		z++;
	}

	return 0;
}

int init_cull_stack(struct ColumnStack *stack) {
	stack->index = 0;
	stack->startIndex = 0;
	stack->occupiedSize = 0;
	stack->data = malloc(32*512*512*sizeof(uint32_t));
	return -(stack->data == NULL);
}

static void reset_cull_solidity(struct ColumnStack *stack, uint64_t (*solid)[512]) {
	Column column;
	uint32_t x, y;

	stack->index = stack->startIndex;
	while (stack->index != stack->occupiedSize) {
		column = stack->data[stack->index++];
		x = COLUMN_GET_X(column);
		y = COLUMN_GET_Y(column);

		solid[x][y] = 0;
	}
}

void finish_cull(struct ColumnStack *stack, uint64_t (*solid)[512]) {
	stack->startIndex = 0;
	reset_cull_solidity(stack, solid);
	stack->index = 0;
	stack->occupiedSize = 0;
}

size_t cull_floating_voxels(uint32_t x, uint32_t y, uint32_t z, int actuallyCull, uint8_t *solidData, struct ColumnStack *stack, uint64_t (*rememberedSolidity)[512], uint64_t (*keepSolid)[512]) {
	Column column;
	uint_fast8_t top, bottom;
	uint_fast8_t i, reachedGround = 0;
	size_t culledVoxels = 0;

	if (!libspades_voxel_get_player_breakability(x, y, z, solidData))
		return 0;

	/* Pack this column. */
	column = detect_column(x, y, z, solidData);
	if (column == 4294967295)
		return 0;

	/* Don't bother if this voxel is already confirmed non-floating. */
	if (keepSolid[x][y] & (uint64_t)1 << ((uint64_t)COLUMN_GET_BOTTOM(column)))
		return 0;

	/* Push that column onto the stack and remember it. */
	stack->data[stack->occupiedSize++] = column;

	rememberedSolidity[x][y] |= (uint64_t)1 << ((uint64_t)COLUMN_GET_BOTTOM(column));
	keepSolid[x][y] |= (uint64_t)1 << ((uint64_t)COLUMN_GET_BOTTOM(column));

	stack->startIndex = stack->index;
	while (stack->index != stack->occupiedSize) {
		column = stack->data[stack->index++];

		x = COLUMN_GET_X(column);
		y = COLUMN_GET_Y(column);
		top = COLUMN_GET_TOP(column);
		bottom = COLUMN_GET_BOTTOM(column);

		if (x < (MAP_SIZE_X - 1) &&
		    detect_and_push_columns(x + 1, y, top, bottom, stack, rememberedSolidity, keepSolid, solidData)) {
			reachedGround = 1;
			break;
		}
		if (x > 0 &&
		    detect_and_push_columns(x - 1, y, top, bottom, stack, rememberedSolidity, keepSolid, solidData)) {
			reachedGround = 1;
			break;
		}
		if (y < (MAP_SIZE_Y - 1) &&
		    detect_and_push_columns(x, y + 1, top, bottom, stack, rememberedSolidity, keepSolid, solidData)) {
			reachedGround = 1;
			break;
		}
		if (y > 0 &&
		    detect_and_push_columns(x, y - 1, top, bottom, stack, rememberedSolidity, keepSolid, solidData)) {
			reachedGround = 1;
			break;
		}
	}

	if (!reachedGround) {
		/* Clear all the bitmasks, we can't trust anything since voxels are being removed */
		stack->index = 0;
		while (stack->index != stack->startIndex) {
			column = stack->data[stack->index++];
			x = COLUMN_GET_X(column);
			y = COLUMN_GET_Y(column);
			rememberedSolidity[x][y] = 0;
			keepSolid[x][y] = 0;
		}
		while (stack->index != stack->occupiedSize) {
			column = stack->data[stack->index++];
			x = COLUMN_GET_X(column);
			y = COLUMN_GET_Y(column);
			top = COLUMN_GET_TOP(column);
			bottom = COLUMN_GET_BOTTOM(column);
			size_t i2 = CALC_I(x, y);

			rememberedSolidity[x][y] = 0;
			keepSolid[x][y] = 0;

			culledVoxels += 1 + bottom - top;
			if (actuallyCull) {
				for (i = top; i <= bottom; i++)
					pvx_voxel_destroy4(solidData, i2, i);
			}
		}
		stack->index = 0;
		stack->occupiedSize = 0;
	} else
		/* Just clear all of rememberedSolidity, and keep index pointed to where it is now
		 * -- we don't want to overwrite old cols, otherwise we can't clear keepSolid */
		reset_cull_solidity(stack, rememberedSolidity);

	return culledVoxels;
}

/*
 * Copyright (c) 2023-2025 totally not a burner and Teodor Draganov.
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
