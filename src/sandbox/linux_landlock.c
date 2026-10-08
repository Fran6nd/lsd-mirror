#include <errno.h>
#include <linux/landlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

static int ll_version(void)
{
	int ver = syscall(
		SYS_landlock_create_ruleset,
		NULL,
		0,
		LANDLOCK_CREATE_RULESET_VERSION
	);

	if (ver < 0)
		ERR("landlock_create_ruleset");

	return ver;
}

static int ll_begin(__u64 fs, __u64 net, __u64 scoped)
{
	struct landlock_ruleset_attr attr = {0};
	int fd;

	attr.handled_access_fs  = fs;
	attr.handled_access_net = net;
	attr.scoped             = scoped;

	fd = syscall(SYS_landlock_create_ruleset, &attr, sizeof(attr), 0);

	if (fd < 0)
		ERR("landlock_create_ruleset");

	return fd;
}

static void ll_apply(int fd)
{
	if (syscall(SYS_landlock_restrict_self, fd, 0) != 0)
		ERR("landlock_restrict_self");

	if (close(fd) != 0 && errno != EINTR)
		ERR("close");
}

static void ll_fs(int fd, const char *path, __u64 access)
{
	struct landlock_path_beneath_attr attr;
	int path_fd;

	while (
		(path_fd = open(path, O_PATH)) < 0 &&
		errno == EINTR
	);

	if (path_fd < 0)
		ERR("open");

	attr.allowed_access = access;
	attr.parent_fd      = path_fd;

	if (syscall(
		SYS_landlock_add_rule,
		fd,
		LANDLOCK_RULE_PATH_BENEATH,
		&attr,
		0
	) != 0)
		ERR("landlock_add_rule");

	if (close(path_fd) != 0 && errno != EINTR)
		ERR("close");
}

#define ACCESS_READ  (LANDLOCK_ACCESS_FS_READ_FILE  | LANDLOCK_ACCESS_FS_READ_DIR)
#define ACCESS_EXEC  (LANDLOCK_ACCESS_FS_EXECUTE)
#define ACCESS_WRITE ( \
	LANDLOCK_ACCESS_FS_WRITE_FILE  | \
	LANDLOCK_ACCESS_FS_TRUNCATE    | \
	LANDLOCK_ACCESS_FS_REMOVE_DIR  | \
	LANDLOCK_ACCESS_FS_REMOVE_FILE | \
	LANDLOCK_ACCESS_FS_MAKE_DIR    | \
	LANDLOCK_ACCESS_FS_MAKE_REG    | \
	LANDLOCK_ACCESS_FS_MAKE_SOCK   | \
	LANDLOCK_ACCESS_FS_MAKE_FIFO   | \
	LANDLOCK_ACCESS_FS_MAKE_SYM    | \
	LANDLOCK_ACCESS_FS_REFER         \
)

static void sandbox_landlock(void)
{
	__u64 fs =
		LANDLOCK_ACCESS_FS_EXECUTE          |
		LANDLOCK_ACCESS_FS_WRITE_FILE       |
		LANDLOCK_ACCESS_FS_READ_FILE        |
		LANDLOCK_ACCESS_FS_READ_DIR         |
		LANDLOCK_ACCESS_FS_REMOVE_DIR       |
		LANDLOCK_ACCESS_FS_REMOVE_FILE      |
		LANDLOCK_ACCESS_FS_MAKE_CHAR        |
		LANDLOCK_ACCESS_FS_MAKE_DIR         |
		LANDLOCK_ACCESS_FS_MAKE_REG         |
		LANDLOCK_ACCESS_FS_MAKE_SOCK        |
		LANDLOCK_ACCESS_FS_MAKE_FIFO        |
		LANDLOCK_ACCESS_FS_MAKE_BLOCK       |
		LANDLOCK_ACCESS_FS_MAKE_SYM         |
		LANDLOCK_ACCESS_FS_REFER            |
		LANDLOCK_ACCESS_FS_TRUNCATE         |
		LANDLOCK_ACCESS_FS_IOCTL_DEV;

	__u64 scoped =
		LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET |
		LANDLOCK_SCOPE_SIGNAL;

	switch (ll_version()) {
	case 1:
		fs &= ~LANDLOCK_ACCESS_FS_REFER;
	case 2:
		fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE;
	case 3:
	case 4:
		fs &= ~LANDLOCK_ACCESS_FS_IOCTL_DEV;
	case 5:
		scoped = 0;
	};

	int fd = ll_begin(fs, 0, scoped);

	ll_fs(fd, "./",      ACCESS_READ);
	ll_fs(fd, "./exec/", ACCESS_EXEC);
	ll_fs(fd, "./rw/",   ACCESS_WRITE);

	ll_fs(fd, "/tmp/", ACCESS_READ | ACCESS_WRITE);

	ll_fs(fd, "/etc/resolv.conf", LANDLOCK_ACCESS_FS_READ_FILE);
	ll_fs(fd, "/etc/hosts",       LANDLOCK_ACCESS_FS_READ_FILE);

	ll_apply(fd);
}
