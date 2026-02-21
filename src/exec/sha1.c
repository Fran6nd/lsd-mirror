/* not a burner's homebaked lua SHA-1 implementation translated to C */
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define ROL32(x, y) (((x) << (y)) | ((x) >> (32-y)))

static uint32_t h32tobe(uint32_t num) {
	uint8_t buf[4];

	buf[0] = num >> 24;
	buf[1] = num >> 16;
	buf[2] = num >>  8;
	buf[3] = num;

	return *(uint32_t *)buf;
}

/* Convert a uint32_t * to big-endian format */
void sha1_arr32_to_be(uint32_t *array, size_t items) {
	size_t i;

	for (i=0;i<items;i++)
		array[i] = h32tobe(array[i]);
}

/* Copy some data in big-endian format into a uint32_t * */
static void cpy_from_be_arr32(const uint8_t *in, uint32_t *array, size_t items) {
	size_t i;

	for (i=0;i<items;i++) {
		array[i]  = (uint32_t)in[i*4]   << 24;
		array[i] |= (uint32_t)in[i*4+1] << 16;
		array[i] |= (uint32_t)in[i*4+2] << 8;
		array[i] |= (uint32_t)in[i*4+3];
	}
}

/* Copy a uint64_t somewhere in big-endian format */
static void cpy_to_be64(uint8_t *out, uint64_t num) {
	out[0] = num >> 56;
	out[1] = num >> 48;
	out[2] = num >> 40;
	out[3] = num >> 32;
	out[4] = num >> 24;
	out[5] = num >> 16;
	out[6] = num >> 8;
	out[7] = num;
}

/* Takes a 64-byte chunk of data and puts its state into hb. */
void sha1_chunk(const void *data, uint32_t hb[5]) {
	unsigned i;
	uint32_t tmp;
	uint32_t words[80];
	uint32_t ha[7];

	memcpy(ha, hb, sizeof(uint32_t [5]));

	cpy_from_be_arr32(data, words, 16);
	for (i=16;i<80;i++)
		words[i] = ROL32(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);

	for (i=0;i<80;i++) {
		if (i < 20) {
			ha[5] = (ha[1] & ha[2]) | (~ha[1] & ha[3]);
			ha[6] = 0x5a827999;
		} else if (i < 40) {
			ha[5] = ha[1] ^ ha[2] ^ ha[3];
			ha[6] = 0x6ed9eba1;
		} else if (i < 60) {
			ha[5] = (ha[1] & ha[2]) | (ha[1] & ha[3]) | (ha[2] & ha[3]);
			ha[6] = 0x8f1bbcdc;
		} else {
			ha[5] = ha[1] ^ ha[2] ^ ha[3];
			ha[6] = 0xca62c1d6;
		}

		tmp = ROL32(ha[0], 5) + ha[4] + ha[5] + ha[6] + words[i];
		ha[4] = ha[3];
		ha[3] = ha[2];
		ha[2] = ROL32(ha[1], 30);
		ha[1] = ha[0];
		ha[0] = tmp;
	}

	for (i=0;i<5;i++)
		hb[i] += ha[i];
}

static const uint32_t sha1_h[5] = {
	0x67452301,
	0xefcdab89,
	0x98badcfe,
	0x10325476,
	0xc3d2e1f0
};

void sha1_set_h(uint32_t hb[5]) {
	memcpy(hb, sha1_h, sizeof(sha1_h));
}

/* Does what you think it does. sizeof(out) == 20 */
void sha1(void *out, const char *data, size_t len) {
	uint8_t buf[64];
	uint64_t lenbits = len*8;

	sha1_set_h(out);

	while (len >= 64) {
		sha1_chunk(data, out);
		data += 64;
		len -= 64;
	}

	/* TODO: memset before memcpy? */
	memcpy(buf, data, len);
	buf[len++] = 0x80;
	memset(buf+len, 0, 64-len);

	if (len <= 64-8) {
		cpy_to_be64(buf+64-sizeof(lenbits), lenbits);
		sha1_chunk(buf, out);

		sha1_arr32_to_be(out, 5);
		return;
	}

	sha1_chunk(buf, out);

	memset(buf, 0, 64-8);
	cpy_to_be64(buf+64-sizeof(lenbits), lenbits);
	sha1_chunk(buf, out);

	sha1_arr32_to_be(out, 5);
}
