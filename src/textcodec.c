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

	for (i=0;i<MAX_PLAYERS;i++) {if (pid_matches(pid, i, st)) {
		const char *sendmsg = msg;

		if (st->p[i].bugMask & QUIRK_UTF8) {
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
}

static void (*next_on_chat)(plid pid, const char *msg, unsigned type, struct State *st);
static void on_chat(plid pid, const char *msg, unsigned type, struct State *st) {
	char *utf8, *ptr;
	size_t len, i;

	if (st->p[pid].bugMask & QUIRK_UTF8 && msg[0] == '\xff' && is_utf8_valid(msg+1))
		return next_on_chat(pid, msg+1, type, st);

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

void hook_textcodec_late(struct State *st) {
	next_send_chat = st->f.send_chat;
	st->f.send_chat = send_chat;
}

void hook_textcodec_early(struct State *st) {
	next_on_chat = st->f.on_chat;
	st->f.on_chat = on_chat;
}
