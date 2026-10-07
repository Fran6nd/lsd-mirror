#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

#define MOUNT(id, source, target, fstype, mntflags) do { \
	if (mount(source, target, fstype, mntflags, NULL) != 0) \
		ERR("mount-"id); \
} while (0)

#define MNT(isdir, dest, src, flags) do { \
	if (isdir) { \
		if (mkdir("/tmp/"dest, 0755) != 0 && errno != EEXIST) \
			ERR("mkdir"); \
	} else { \
		int fd = open("/tmp/"dest, O_RDONLY | O_CREAT, 0755); \
		if (fd < 0) ERR("open-"dest); \
		if (close(fd) != 0 && errno != EINTR) ERR("close-"dest); \
	} \
\
	if (mount( \
		src, \
		"/tmp/"dest, \
		NULL, \
		MS_SILENT | MS_BIND | MS_REC, \
		NULL \
	) == 0) { \
		MOUNT( \
			"remount-"dest, \
			NULL, \
			"/tmp/"dest, \
			NULL, \
			MS_SILENT | MS_BIND | MS_REC | \
			MS_REMOUNT | MS_NODEV | MS_NOSUID | flags \
		); \
	} else { \
		if (isdir) { \
			if (rmdir("/tmp/"dest) != 0) \
				ERR("rmdir"); \
		} else { \
			if (unlink("/tmp/"dest) != 0) \
				ERR("rmdir"); \
		} \
	} \
} while (0)

static void write_file(const char *path, const void *buf, ssize_t nbytes)
{
	int fd;

	if ((fd = open(path, O_WRONLY)) < 0)
		ERR("open1");

	if (write(fd, buf, nbytes) != nbytes)
		ERR("write1");

	if (close(fd) != 0)
		ERR("close1");
}

static void drop_caps(void)
{
	struct __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
	struct __user_cap_data_struct data[2] = {{0, 0, 0}, {0, 0, 0}};

	if (syscall(SYS_capset, &hdr, data) != 0)
		ERR("capset");
}

static void sandbox_unshare(void)
{
	char buf[5+10+3+1];
	int buflen;

	uid_t uid = getuid();
	gid_t gid = getgid();

	if (unshare(
		CLONE_FILES   | \
		CLONE_NEWIPC  | \
		CLONE_NEWNS   | \
		CLONE_NEWPID  | \
		CLONE_NEWUSER | \
		CLONE_SYSVSEM
	) != 0)
		ERR("unshare");

	write_file("/proc/self/setgroups", "deny\n", sizeof("deny\n")-1);

	buflen = sprintf(buf, "1000 %u 1\n", uid);
	write_file("/proc/self/uid_map", buf, buflen);

	buflen = sprintf(buf, "1000 %u 1\n", gid);
	write_file("/proc/self/gid_map", buf, buflen);

	MOUNT("/", NULL, "/", NULL, MS_SILENT | MS_REC | MS_SLAVE);
	MOUNT(
		"/-tmpfs", NULL, "/tmp", "tmpfs",
		MS_SILENT | MS_NODEV | MS_NOSUID | MS_NOEXEC
	);

	if (mkdir("/tmp/tmp", 0755) != 0) ERR("mkdir");
	if (mkdir("/tmp/etc", 0755) != 0) ERR("mkdir");

	MNT(0, "etc/resolv.conf", "/etc/resolv.conf", MS_NOEXEC | MS_RDONLY);
	MNT(0, "etc/hosts",       "/etc/hosts",       MS_NOEXEC | MS_RDONLY);

	MNT(1, "lsd",      ".",    MS_NOEXEC | MS_RDONLY);
	MNT(1, "lsd/rw",   "rw",   MS_NOEXEC);
	MNT(1, "lsd/exec", "exec", MS_RDONLY);

	if (syscall(SYS_pivot_root, "/tmp", "/tmp")) ERR("pivot_root");
	if (umount2("/", MNT_DETACH)) ERR("umount2");
	if (chdir("/lsd")) ERR("chdir");

	drop_caps();
}
