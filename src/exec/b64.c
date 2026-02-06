#include <stdint.h>
#include <stddef.h>

static uint8_t get_bits(uint8_t val, uint8_t off) {
	if (off > 2)
		return (val << (off-2)) & 63;

	return (val >> (2-off)) & 63;
}

#define MAP "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
void b64enc(char *out, const uint8_t *in, size_t inSize) {
	size_t i,j = 0;
	for (i=0;i<=(inSize+5)/6*48;i+=6) {
		if (i/8+1 > inSize)
			out[j++] = '=';
		else {
			uint8_t val;
			uint8_t bitoff = i % 8;
			uint8_t op0 = in[i/8];
			uint8_t op1 = 0;

			if (i/8+1 < inSize)
				op1 = in[i/8+1];

			val = get_bits(op0, bitoff);
			if (bitoff > 2)
				val = val | (op1 >> (10 - bitoff));

			out[j++] = MAP[val];
		}
	}
}
