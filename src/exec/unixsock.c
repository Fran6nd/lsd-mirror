#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

void close_sock(int fd) {
	while (close(fd) == -1 && errno == EINTR);
}

/* TODO: use strerror(_r) on this? directly raise a lua error? */
int create_unix_sock(const char *path) {
        int fd;
	size_t pathlen;
	struct sockaddr_un addr = {0};

	pathlen = strlen(path)+1;
	if (pathlen > sizeof(addr.sun_path)) {
		errno = 0;
		return -1;
	}

	addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, path, pathlen);

	/* TODO: use fcntl instead of SOCK_NONBLOCK and SOCK_CLOEXEC? */
	if ((fd = socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) == -1)
		return -1;

        if (bind(fd, (struct sockaddr *)&addr, offsetof(struct sockaddr_un, sun_path)+pathlen) == -1) {
		close_sock(fd);
		return -1;
	}

        if (listen(fd, 16) == -1) {
		close_sock(fd);
		return -1;
	}

	return fd;
}

/* TODO: dual v4/v6 */
int create_tcp_sock(const char *listenaddr, const char *port) {
	int fd, status, y = 1;
	struct addrinfo hints = {0};
	struct addrinfo *res;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (listenaddr == NULL)
		hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(listenaddr, port, &hints, &res)) != 0)
		return -1;

	if ((fd = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC | SOCK_NONBLOCK, res->ai_protocol)) == -1) {
		freeaddrinfo(res);
		return -1;
	}

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y));

        if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
		close_sock(fd);
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);

        if (listen(fd, 16) == -1) {
		close_sock(fd);
		return -1;
	}

	return fd;
}

int accept_sock(int fd) {
	int con = accept(fd, NULL, NULL);

	if (con != -1) {
		int flags = fcntl(con, F_GETFL) | O_NONBLOCK;
		if (fcntl(con, F_SETFL, flags) == -1) {
			close_sock(con);
			return -1;
		}
	}

	return con;
}

ssize_t send_sock(int fd, const char *buf, size_t len) {
	return send(fd, buf, len, MSG_NOSIGNAL);
}

ssize_t recv_sock(int fd, char *buf, size_t len) {
	return recv(fd, buf, len, 0);
}
