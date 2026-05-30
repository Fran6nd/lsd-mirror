/* textcodec.c -- Transparently handle grand mess of incompatible CP-437 implementations */
#include <stdio.h>
#include "state.h"
#include "textcodec_utf8.h"
#include "textcodec_cp437.h"
#ifdef WITH_ANYASCII
#include "anyascii/impl/c/anyascii.c"
#endif

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
int pid_matches(plid broadcast, plid pid, struct State *st);

static const uint8_t imgtbl[256] = {
	/* 0x07 and 0x08 are swapped */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x07, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

typedef void (*OnCodepoint)(long cp, void *udata);

static void parse_utf8(const char *msg, OnCodepoint oncp, void *udata) {
	size_t i;
	int state = 0;
	long cp = 0;

	for (i=0;msg[i]!='\0';i++) {
		state = utf8_decode(state, &cp, (unsigned char)msg[i]);

		switch (state) {
		case UTF8_REJECT: /* -1 */
			cp = 0xfffd;
			state = 0;
			/* fall-through */
		case UTF8_ACCEPT: /* 0 */
			oncp(cp, udata);
			cp = 0;
		}
	}

	if (state != UTF8_ACCEPT)
		oncp(0xfffd, udata);
}

static int is_utf8_valid(const char *msg) {
	size_t i;
	int state = 0;
	long junk;

	for (i=0;msg[i]!='\0';i++) {
		state = utf8_decode(state, &junk, (unsigned char)msg[i]);

		if (state == UTF8_REJECT)
			return 0;
	}

	return state == UTF8_ACCEPT;
}

static void getlen_ascii(long cp, void *udata) {
#ifdef WITH_ANYASCII
	const char *junk;

	*((size_t *)udata) += anyascii(cp, &junk);
#else
	(void)cp;
	*((size_t *)udata) += 1;
#endif
}

static void getlen_cp437(long cp, void *udata) {
#ifdef WITH_ANYASCII
	const char *junk;

	if (codepoint_to_cp437(cp) >= 0) {
		*((size_t *)udata) += 1;
		return;
	}

	*((size_t *)udata) += anyascii(cp, &junk);
#else
	(void)cp;
	*((size_t *)udata) += 1;
#endif
}

/* TODO: strip most of <0x20 */
static void to_ascii(long cp, void *udata) {
	char **ascii = udata;
#ifdef WITH_ANYASCII
	const char *str;
	size_t len;

	len = anyascii(cp, &str);
	memcpy(*ascii, str, len);
	*ascii += len;
#else
	*((*ascii)++) = cp > 0x7f ? '?' : cp;
#endif
}

static void to_cp437(long cp, void *udata) {
	char **cp437 = udata;
#ifdef WITH_ANYASCII
	const char *str;
	size_t len;
#endif
	int chr;

	chr = codepoint_to_cp437(cp);
	if (chr >= 0) {
		*((*cp437)++) = chr;
		return;
	}

#ifdef WITH_ANYASCII
	/* TODO: how does anyascii handle \x7f? */
	len = anyascii(cp, &str);
	memcpy(*cp437, str, len);
	*cp437 += len;
#else
	*((*cp437)++) = '?';
#endif
}

/* TODO: move charset conversion from send_chat to server_msg()/player_msg()? */
static void (*next_send_chat)(plid pid, const char *msg, unsigned type, plid from, struct State *st);
static void send_chat(plid pid, const char *msg, unsigned type, plid from, struct State *st) {
	plid i;
	char *ascii = NULL;
	char *cp437 = NULL;
	char *utf8 = NULL;
	char *utf8img = NULL;

	for (i=0;i<MAX_PLAYERS;i++) {if (pid_matches(pid, i, st)) {
		const char *sendmsg = msg;

		if (st->p[i].bugMask & QUIRK_UTF8_COLOR_IMG) {
			if (utf8img == NULL) {
				size_t msglen = strlen(msg)+1;
				size_t j;

				utf8img = malloc(1+msglen);
				if (utf8img == NULL)
					ERR("malloc");

				utf8img[0] = '\xff';
				for (j=0;j<msglen+1;j++)
					utf8img[j+1] = imgtbl[(uint8_t)msg[j]];
			}

			sendmsg = utf8img;
		} else if (st->p[i].bugMask & QUIRK_UTF8) {
			if (utf8 == NULL) {
				size_t msglen = strlen(msg)+1;

				utf8 = malloc(1+msglen);
				if (utf8 == NULL)
					ERR("malloc");

				utf8[0] = '\xff';
				memcpy(utf8+1, msg, msglen);
			}

			sendmsg = utf8;
		} else if (st->p[i].bugMask & QUIRK_ASCII) {
			if (ascii == NULL) {
				/* 1 byte for the NUL terminator */
				size_t len = 1;
				char *ptr;

				parse_utf8(msg, getlen_ascii, &len);

				ascii = malloc(len);
				if (ascii == NULL)
					ERR("malloc");

				ptr = ascii;
				parse_utf8(msg, to_ascii, &ptr);

				ascii[len-1] = '\0';
			}

			sendmsg = ascii;
		} else {
			if (cp437 == NULL) {
				/* 1 byte for the NUL terminator */
				size_t len = 1;
				char *ptr;

				parse_utf8(msg, getlen_cp437, &len);

				cp437 = malloc(len);
				if (cp437 == NULL)
					ERR("malloc");

				ptr = cp437;
				parse_utf8(msg, to_cp437, &ptr);

				cp437[len-1] = '\0';
			}

			sendmsg = cp437;
		}

		next_send_chat(i, sendmsg, type, from, st);
	}}

	if (ascii)
		free(ascii);

	if (cp437)
		free(cp437);

	if (utf8)
		free(utf8);

	if (utf8img)
		free(utf8img);
}

static void (*next_on_chat)(plid pid, const char *msg, unsigned type, struct State *st);
static void on_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	char *utf8, *ptr;
	size_t len, i;

	if (st->p[pid].bugMask & QUIRK_UTF8 && msg[0] == '\xff' && is_utf8_valid(msg+1)) {
		if (!(st->p[pid].bugMask & QUIRK_UTF8_COLOR_IMG))
			return next_on_chat(pid, msg+1, type, st);

		len = strlen(msg);
		utf8 = malloc(len + 1);
		if (utf8 == NULL)
			ERR("malloc");

		for (i=0;i<len+1;i++)
			utf8[i] = imgtbl[(uint8_t)utf8[i]];

		next_on_chat(pid, utf8, type, st);

		free(utf8);
	}

	len = strlen(msg);
	utf8 = malloc(len*3 + 1);
	if (utf8 == NULL)
		ERR("malloc");

	ptr = utf8;

	if (st->p[pid].bugMask & QUIRK_OS_CP437) {
		for (i=0;i<len+1;i++)
			ptr = utf8_encode(ptr, openspades_cp437_to_unicode[(unsigned char)msg[i]]);
	} else {
		for (i=0;i<len+1;i++)
			ptr = utf8_encode(ptr, canon_cp437_to_unicode[(unsigned char)msg[i]]);
	}

	next_on_chat(pid, utf8, type, st);

	free(utf8);
}

static void (*next_on_version)(plid pid, unsigned idChar, unsigned major, unsigned minor, unsigned patch, const char *msg, size_t msglen, struct State *st);
static void on_version(plid pid, unsigned idChar, unsigned major, unsigned minor, unsigned patch, const char *msg, size_t msglen, struct State *st) {
	char *utf8, *ptr;
	size_t i;

	/* By now server heuristics haven't determined presence of QUIRKs,
	 * so it's assumed that the client has QUIRK_UTF8
	 */
	if (msg[0] == '\xff' && is_utf8_valid(msg+1))
		return next_on_version(pid, idChar, major, minor, patch, msg+1, msglen, st);

	utf8 = malloc(msglen*3);
	if (utf8 == NULL)
		ERR("malloc");

	ptr = utf8;

	/* QUIRK_OS_CP437 is assumed here */
	for (i=0;i<msglen;i++)
		ptr = utf8_encode(ptr, openspades_cp437_to_unicode[(unsigned char)msg[i]]);

	next_on_version(pid, idChar, major, minor, patch, utf8, ptr-utf8, st);

	free(utf8);
}

void hook_textcodec_late(struct State *st) {
	next_send_chat = st->f.send_chat;
	st->f.send_chat = send_chat;
}

void hook_textcodec_early(struct State *st) {
	next_on_chat = st->f.on_chat;
	next_on_version = st->f.on_version;

	st->f.on_chat = on_chat;
	st->f.on_version = on_version;
}
