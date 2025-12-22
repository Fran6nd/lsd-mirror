#ifndef PYPVX_BITMASK_H
#define PYPVX_BITMASK_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAP_SIZE_X 512
#define MAP_SIZE_Y 512
#define MAP_SIZE_Z 64

#define CALC_I(x, y) ((x)*64 + (y)*512*64)

struct BitmaskUData {
	uint8_t *solidData;
	uint8_t *colorData;
};

static inline int pvx_create_bitmask(struct BitmaskUData *data, uint_fast32_t xSize, uint_fast32_t ySize, uint_fast32_t zSize) {
	data->solidData = malloc((xSize*ySize*zSize+7)/8 + xSize*ySize*zSize*3);
	if (data->solidData == NULL) return -1;
	data->colorData = data->solidData+(xSize*ySize*zSize+7)/8;
	memset(data->solidData, 0, (xSize*ySize*zSize+7)/8);
	return 0;
}

static inline void pvx_destroy_bitmask(struct BitmaskUData *data) {
	free(data->solidData);
}

static inline void pvx_voxel_create4(uint8_t *outBuf,
                              const size_t i,
                              const uint_fast32_t z) {
	size_t i2 = z + i;
	outBuf[i2 >> 3] |= 1 << (i2 & 7);
}

static inline void pvx_voxel_destroy4(uint8_t *outBuf,
                               const size_t i,
                               const uint_fast32_t z) {
	size_t i2 = z + i;
	outBuf[i2 >> 3] &= ~(1 << (i2 & 7));
}

static inline int pvx_voxel_get_solidity4(const uint8_t *outBuf,
                                   const size_t i,
                                   const uint_fast32_t z) {
	size_t i2 = z + i;
	return !!(outBuf[i2 >> 3] & (1 << (i2 & 7)));
}

static inline void pvx_voxel_color5(uint8_t *outBuf, const uint8_t *color, size_t i, uint_fast32_t z) {
	memcpy(outBuf+(z+i)*3, color, 3);
}

#endif
