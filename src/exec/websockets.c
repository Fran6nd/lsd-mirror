#include <stdint.h>
#include <string.h>

static uint32_t h16tobe(uint16_t num) {
	uint8_t buf[2];

	buf[0] = num >> 8;
	buf[1] = num;

	return *(uint16_t *)buf;
}

#define U8NUM (uint16_t)((uint8_t *)&num)
static uint32_t be16toh(uint16_t num) {
	return U8NUM[0] << 8 | U8NUM[1];
}
#undef U8NUM

static uint32_t h64tobe(uint64_t num) {
	uint8_t buf[8];

	buf[0] = num >> 56;
	buf[1] = num >> 48;
	buf[2] = num >> 40;
	buf[3] = num >> 32;
	buf[4] = num >> 24;
	buf[5] = num >> 16;
	buf[6] = num >>  8;
	buf[7] = num;

	return *(uint64_t *)buf;
}

#define U8NUM (uint64_t)((uint8_t *)&num)
static uint32_t be64toh(uint16_t num) {
	return U8NUM[0] << 56 | U8NUM[1] << 48 | U8NUM[2] << 40 | U8NUM[3] << 32 |
	       U8NUM[4] << 24 | U8NUM[5] << 16 | U8NUM[6] <<  8 | U8NUM[7];
}
#undef U8NUM

#define HANDSHAKE_BAD \
	"HTTP/1.1 400 Bad Request\r\n"\
	"Connection: close\r\n"\
	"Sec-WebSocket-Version: 13\r\n"\
	"\r\n"

#define HANDSHAKE_PREFIX \
	"HTTP/1.1 101 Switching Protocols\r\n"\
	"Upgrade: websocket\r\n"\
	"Connection: Upgrade\r\n"

#define HANDSHAKE_ACCEPT_PFX \
	"Sec-WebSocket-Accept: "

#define WS_GOTFLAG_GET 1
#define WS_GOTFLAG_UPGRADE 2
#define WS_GOTFLAG_CONNECTION 4
#define WS_GOTFLAG_VERSION 8

struct WS_Headers {
	unsigned gotFlags;
	char keyBuf[24];
	char hdrBuf[46];
	uint8_t hdrBufLen;
};

#define HDRBUF_STR_PFX_EQ(str) (!memcmp(hdr->hdrBuf, str, sizeof(str)-1))
#define HDRBUF_STR_EQ(str) (hdr->hdrBufLen == sizeof(str)-1 && !memcmp(hdr->hdrBuf, str, sizeof(str)-1))
int websockets_read_handshake_hdrs(const char *in, size_t size, struct WS_Headers *hdr) {
	while (size) {
		const char *end = memchr(in, '\n', size);
		size_t cpy;

		if (end == NULL)
			cpy = size;
		else
			cpy = end-in;

		if (cpy > sizeof(hdr->hdrBuf)-hdr->hdrBufLen-1) {
			cpy = sizeof(hdr->hdrBuf)-hdr->hdrBufLen-1;
			in += cpy;
			size -= cpy;

			/* Prevent headers longer than we care to parse from screwing things over */
			hdr->hdrBuf[0] = '\0';
			hdr->hdrBufLen = 1;

			continue;
		}

		memcpy(hdr->hdrBuf+hdr->hdrBufLen, in, cpy);
		hdr->hdrBufLen += cpy;

		if (end)
			cpy++;

		in += cpy;
		size -= cpy;

		if (end) {
			if (hdr->hdrBuf[hdr->hdrBufLen-1] == '\r')
				hdr->hdrBufLen--;
			hdr->hdrBuf[hdr->hdrBufLen] = '\0';

#if 0
			/* Attempt to debug parsed headers -- you'll have to #include <unistd.h> */
			write(2, "hdr: <", 6);
			write(2, hdr->hdrBuf, hdr->hdrBufLen);
			write(2, ">\n", 2);
#endif

			if (hdr->gotFlags & WS_GOTFLAG_GET) {
				if (HDRBUF_STR_EQ("Upgrade: websocket"))
					hdr->gotFlags |= WS_GOTFLAG_UPGRADE;
				if (HDRBUF_STR_EQ("Connection: Upgrade"))
					hdr->gotFlags |= WS_GOTFLAG_CONNECTION;
				if (HDRBUF_STR_PFX_EQ("Sec-WebSocket-Version: 13"))
					hdr->gotFlags |= WS_GOTFLAG_VERSION;
				if (HDRBUF_STR_PFX_EQ("Sec-WebSocket-Key: ")) {
					if (hdr->hdrBufLen != 43)
						return -1;
					memcpy(hdr->keyBuf, hdr->hdrBuf+19, 24);
				}
			} else if (HDRBUF_STR_PFX_EQ("GET /"))
				hdr->gotFlags |= WS_GOTFLAG_GET;
			else
				/* Stop parsing headers, got error */
				return -1;

			/* Stop parsing headers, reached end */
			if (hdr->hdrBufLen == 0)
				return hdr->gotFlags == 15 ? 1 : -1;

			hdr->hdrBufLen = 0;
		}
	}

	return 0;
}

#define HANDSHAKE_BUF_MAX_LEN sizeof(HANDSHAKE_PREFIX)-1+sizeof(HANDSHAKE_ACCEPT_PFX)-1+28+2+2+1

#define MEMCPY_LITERAL(to, from) memcpy(to, from, sizeof(from)-1);
void sha1_chunk(const void *data, uint32_t hb[5]);
void sha1_arr32_to_be(uint32_t *array, size_t items);
void sha1_set_h(uint32_t hb[5]);
void b64enc(char *out, const uint8_t *in, size_t inSize);
size_t websockets_fill_handshake_buf(char *out, struct WS_Headers *hdr) {
	if (hdr->gotFlags != 15) {
		strcpy(out, HANDSHAKE_BAD);
		return sizeof(HANDSHAKE_BAD)-1;
	}

	strcpy(out, HANDSHAKE_PREFIX);

	if (hdr->keyBuf[0]) {
		char buf[64];
		uint32_t hb[5];
		sha1_set_h(hb);

		memcpy(buf, hdr->keyBuf, 24);
		MEMCPY_LITERAL(buf+24, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11\x80\x00\x00\x00");
		sha1_chunk(buf, hb);

		memset(buf, 0, 64-8);
		/* 60*8 as a big-endian uint64_t */
		MEMCPY_LITERAL(buf+64-8, "\x00\x00\x00\x00\x00\x00\x01\xe0");
		sha1_chunk(buf, hb);
		sha1_arr32_to_be(hb, 5);

		b64enc(buf, (uint8_t *)hb, sizeof(hb));
		buf[28] = '\0';

		strcat(out, HANDSHAKE_ACCEPT_PFX);
		strcat(out, buf);
		strcat(out, "\r\n");
	}

	strcat(out, "\r\n");
	return strlen(out);
}

#define WS_OPCODE_CONT  0
#define WS_OPCODE_TEXT  1
#define WS_OPCODE_BLOB  2

#define WS_OPCODE_CLOSE 8
#define WS_OPCODE_PING  9
#define WS_OPCODE_PONG  10

#define WS_OPCODE_ERR   255

/* memset(&frame, 0, sizeof frame); frame.final = 1; to init */
struct WS_Frame {
	uint64_t dataLen;
	size_t contHdrLen;
	uint8_t mask[4];
	unsigned opcode;
	int final;
	int masked;

	uint8_t tmpBuf[12];
	uint8_t tmpBufLen;
};

/* Returns number of bytes that need to be fed to websockets_read_frame_hdr_continued */
static int websockets_read_frame_hdr(struct WS_Frame *frame, const uint8_t *in) {
	unsigned opcode = in[0] & 0x0f;
	if (opcode == WS_OPCODE_CONT && frame->final)
		goto err;
	if ((opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BLOB) && !frame->final)
		goto err;
	if ((opcode > 2 && opcode < 8) || (opcode > 9))
		goto err;

	frame->opcode = opcode;
	if (opcode < 3) {
		frame->final = !!(in[0] & 0x80);
		frame->dataLen = in[1] & 0x7f;
	} else {
		frame->dataLen = in[1] & 0x7f;
		if (frame->dataLen > 125)
			goto err;
	}

	frame->masked = !!(in[1] & 0x80);
	frame->contHdrLen = frame->masked*4;
	switch (frame->dataLen) {
	case 126:
		frame->contHdrLen += 2;
		break;
	case 127:
		frame->contHdrLen += 8;
		break;
	}

	return 0;

err:
	frame->opcode = WS_OPCODE_ERR;
	return -1;
}

#define READ_VAL(var) do {memcpy(&(var), in, sizeof(var)); in += sizeof(var);} while (0)
static void websockets_read_frame_hdr_continued(struct WS_Frame *frame, const uint8_t *in) {
	uint16_t shrt;

	switch (frame->dataLen) {
	case 126:
		READ_VAL(shrt);
		frame->dataLen = be16toh(shrt);
		break;
	case 127:
		READ_VAL(frame->dataLen);
		frame->dataLen = be64toh(frame->dataLen);
		break;
	}

	if (frame->masked)
		READ_VAL(frame->mask);

	frame->contHdrLen = 0;
}

#define WS_EVENT_RECV  1
#define WS_EVENT_CLOSE 2
#define WS_EVENT_PING  4
#define WS_EVENT_ERR   8

#define WS_EVENT_FLAG_RECV_START 1
#define WS_EVENT_FLAG_RECV_END   2

struct WS_Event {
	unsigned type;
	unsigned flags;
	void *recvData;
	size_t recvLen;
};

static void mask_data(uint8_t mask[4], uint8_t *data, size_t size) {
	uint64_t i;
	uint8_t tmp;

	for (i=0;i<size;i++)
		data[i] ^= mask[i%4];

	for (i=0;i<size%4;i++) {
		tmp = mask[0];
		mask[0] = mask[1];
		mask[1] = mask[2];
		mask[2] = mask[3];
		mask[3] = tmp;
	}
}

static void fill_event(struct WS_Frame *frame, struct WS_Event *event, uint8_t *in, size_t size) {
	/* TODO: handle recv for other opcodes */
	switch (frame->opcode) {
	case WS_OPCODE_BLOB:
	case WS_OPCODE_TEXT:
		/* TODO: only set start flag if last was final? */
		event->flags |= WS_EVENT_FLAG_RECV_START;
		frame->opcode = WS_OPCODE_CONT;
		/* fall-through */
	case WS_OPCODE_CONT:
		event->type = WS_EVENT_RECV;

		event->recvData = in;
		event->recvLen = frame->dataLen < size ? frame->dataLen : size;
		frame->dataLen -= event->recvLen;

		if (frame->final && frame->dataLen == 0)
			event->flags |= WS_EVENT_FLAG_RECV_END;

		if (frame->masked)
			mask_data(frame->mask, event->recvData, event->recvLen);

		break;
	case WS_OPCODE_CLOSE:
		event->type = WS_EVENT_CLOSE;

		/* TODO: dedup */
		event->recvData = in;
		event->recvLen = frame->dataLen < size ? frame->dataLen : size;
		frame->dataLen -= event->recvLen;

		if (frame->masked)
			mask_data(frame->mask, event->recvData, event->recvLen);
		break;
	case WS_OPCODE_PING:
		event->type = WS_EVENT_PING;

		event->recvData = in;
		event->recvLen = frame->dataLen < size ? frame->dataLen : size;
		frame->dataLen -= event->recvLen;

		if (frame->masked)
			mask_data(frame->mask, event->recvData, event->recvLen);
		break;
	default:
		event->type = WS_EVENT_ERR;
		break;
	}
}

size_t websockets_consume(struct WS_Frame *frame, struct WS_Event *event, uint8_t *in, size_t size) {
	size_t init_size = size;

	event->type = 0;
	event->flags = 0;
	event->recvData = NULL;
	event->recvLen = 0;

	while (size) {
		if (!frame->contHdrLen) {
			if (frame->dataLen) {
				fill_event(frame, event, in, size);
				size -= event->recvLen;
				break;
			}

			if (frame->tmpBufLen) {
				frame->tmpBuf[1] = (size--,*in++);
				if (websockets_read_frame_hdr(frame, frame->tmpBuf) != 0) {
					event->type = WS_EVENT_ERR;
					break;
				}
				frame->tmpBufLen = 0;
			} else {
				if (size >= 2) {
					size -= 2;
					if (websockets_read_frame_hdr(frame, in) != 0) {
						event->type = WS_EVENT_ERR;
						break;
					}
					in += 2;
				} else
					frame->tmpBuf[frame->tmpBufLen++] = (size--,*in++);
			}
		}

		if (frame->contHdrLen) {
			uint8_t cpy = size < frame->contHdrLen-frame->tmpBufLen ? size : frame->contHdrLen-frame->tmpBufLen;

			memcpy(frame->tmpBuf+frame->tmpBufLen, in, cpy);
			frame->tmpBufLen += cpy;

			in += cpy;
			size -= cpy;

			if (frame->tmpBufLen == frame->contHdrLen) {
				websockets_read_frame_hdr_continued(frame, frame->tmpBuf);
				fill_event(frame, event, in, size);
				size -= event->recvLen;
				frame->tmpBufLen = 0;
				break;
			}
		}
	}

	return init_size - size;
}

size_t websockets_create_frame(uint8_t *buf, uint32_t opcode, int final, uint64_t size) {
	uint16_t shrt;

	*buf++ = (!!final << 7) | opcode;

	if (size < 126) {
		*buf++ = size;
		return 2;
	}

	if (size < 65536) {
		*buf++ = 126;
		shrt = h16tobe(size);
		memcpy(buf, &shrt, sizeof(shrt));

		return 4;
	}

	*buf++ = 127;
	size = h64tobe(size);
	memcpy(buf, &size, sizeof(size));

	return 10;
}

size_t websockets_create_close_frame(uint8_t *buf, uint16_t code) {
	websockets_create_frame(buf, WS_OPCODE_CLOSE, 1, 2);
	code = h16tobe(code);
	memcpy(buf+2, &code, 2);
	return 4;
}

#if 0
void process_input(int fd, void *data, size_t size) {
	struct WS_Frame frame = {0};
	struct WS_Event event;
	frame.final = 1;

	while (size) {
		size_t consumed = websockets_consume(&frame, &event, data, size);
		data += consumed;
		size -= consumed;

		switch (event.type) {
			char buf[2+125];
			uint16_t code;

		case WS_EVENT_RECV:
			write(1, event.recvData, event.recvLen);
			break;
		case WS_EVENT_CLOSE:
			websockets_create_frame(buf, WS_OPCODE_CLOSE, 1, event.recvLen >= 2 ? 2 : 0);
			memcpy(buf+2, event.recvData, event.recvLen);
			send(fd, buf, 2+event.recvLen, MSG_NOSIGNAL);
			close(fd);
			break;
		case WS_EVENT_PING:
			websockets_create_frame(buf, WS_OPCODE_PING, 1, event.recvLen);
			memcpy(buf+2, event.recvData, event.recvLen);
			send(fd, buf, 2+event.recvLen, MSG_NOSIGNAL);
			break;
		case WS_EVENT_ERR:
			send(fd, buf, websockets_create_close_frame(buf, 1002), MSG_NOSIGNAL);
			close(fd);
			break;
		}
	}
}
#endif
