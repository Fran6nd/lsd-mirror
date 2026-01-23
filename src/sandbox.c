#ifdef __linux__
#define _GNU_SOURCE
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <sched.h>
#include <seccomp.h>
#define WITH_LIBSECCOMP
#define WITH_UNSHARE
#endif
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)
#define PUTSERR(x) do {fputs(x"\n", stderr); exit(EXIT_FAILURE);} while (0)

#ifdef __linux__
#ifdef WITH_UNSHARE
#define MOUNT(id, source, target, fstype, mntflags, data) do{if (mount(source, target, fstype, mntflags, data)) ERR("mount-"id);}while(0)
/* TODO: should it just ignore bind-path fail instead of mkdir'ing? */
#define MNT(path, flags) do { \
	if (mkdir(path, 0755) != 0 && errno != EEXIST) ERR("mkdir"); \
	MOUNT("bind-"path, path, "/tmp/"path, NULL, MS_SILENT | MS_BIND | MS_REC, NULL); \
	MOUNT("remount-"path, NULL, "/tmp/"path, NULL, MS_SILENT | MS_REMOUNT | MS_BIND | MS_NODEV | MS_NOSUID | MS_REC | flags, NULL); \
} while (0)
static void pivot(void) {
	/* TODO: is landlock worth using? */
	struct __user_cap_header_struct hdr = {_LINUX_CAPABILITY_VERSION_3, 0};
	struct __user_cap_data_struct data[2] = {{0, 0, 0}, {0, 0, 0}};
	uid_t uid = getuid();
	gid_t gid = getgid();
	char buf[5+10+3+1];
	int buflen;
	int fd;

	if (unshare(CLONE_FILES | CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUSER | CLONE_SYSVSEM)) ERR("unshare");

	buflen = sprintf(buf, "1000 %u 1\n", uid);
	fd = open("/proc/self/uid_map", O_WRONLY);
	if (fd < 0) ERR("open0");
	if (write(fd, buf, buflen) < 0) ERR("write0");
	if (close(fd)) ERR("close0");

	fd = open("/proc/self/setgroups", O_WRONLY);
	if (fd < 0) ERR("open1");
	if (write(fd, "deny\n", sizeof("deny\n")-1) < 0) ERR("write1");
	if (close(fd)) ERR("close1");

	buflen = sprintf(buf, "1000 %u 1\n", gid);
	fd = open("/proc/self/gid_map", O_WRONLY);
	if (fd < 0) ERR("open2");
	if (write(fd, buf, buflen) < 0) ERR("write2");
	if (close(fd)) ERR("close2");

	MOUNT("/", NULL, "/", NULL, MS_SILENT | MS_REC | MS_SLAVE, NULL);
	MNT(".", MS_NOEXEC | MS_RDONLY);
	MNT("rw", MS_NOEXEC);
	MNT("exec", MS_RDONLY);

	if (syscall(SYS_pivot_root, "/tmp", "/tmp")) ERR("pivot_root");
	if (umount2("/", MNT_DETACH)) ERR("umount2");
	if (chdir("/")) ERR("chdir");

	if (syscall(SYS_capset, &hdr, data)) ERR("capset");
}
#endif
#endif

void sandbox(void) {
#ifdef __OpenBSD__
	unveil("./maps/", "r");
	unveil("./scripts/", "r");
	unveil("./config.lua", "r");
	unveil("./exec/", "rx");
	unveil("./rw/", "rwc");
	unveil(NULL, NULL);

	pledge("stdio rpath inet prot_exec flock", "");
#endif

#ifdef __linux__
#ifdef WITH_UNSHARE
	/* "unveil" + other trash, roughly based on pivot_root_demo.c; see pivot_root(2) */
	pivot();
#endif

#ifdef WITH_LIBSECCOMP
	/* "pledge" */
	size_t i;
	/* TODO: clock_gettime is suspiciously absent */
	static const char *const calls[] = {
		/* needed for core/lua */
		"read",
		"open",
		"close",
		"fstat",
		"poll",
		"lseek",
		"mmap",
		"mprotect",
		"munmap",
		"brk",
		"ioctl",
		"readv",
		"writev",
		"mremap",
		"socket",
		"sendmsg",
		"recvmsg",
		"bind",
		"getsockname",
		"setsockopt",
		"fcntl",
		"restart_syscall",
		"exit_group",
		"getrandom",
		"rt_sigreturn",
		/* needed for lsqlite3 */
		"getcwd",
		"lstat",
		"getpid",
		"stat",
		"pread64",
		"geteuid",
		"pwrite64",
		"fdatasync",
		"unlink",
		"ftruncate",
		/* needed for linenoise */
		"write",
#if 0
		/* needed for openmp */
		"sched_yield",
		"sched_getaffinity",
		"membarrier",
		"sched_setaffinity",
		"rt_sigaction",
		"rt_sigprocmask",
		"prlimit64",
		"clone",
		"tkill",
		"futex",
		"getuid",
#endif
		NULL
	};

	/* For figuring out what syscalls the thing'd use:
	 * scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_LOG);
	 * and then check dmesg and look at /usr/include/asm/unistd_64.h
	 * Or you could just use strace.
	 */
	scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);

	if (!ctx)
		PUTSERR("seccomp_init: failed for some reason");

	for (i=0;calls[i]!=NULL;i++) {
		/* does this set errno? */
		if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, seccomp_syscall_resolve_name(calls[i]), 0))
			ERR("seccomp_rule_add");
	}

	if (seccomp_load(ctx))
		ERR("seccomp_load");
	
	seccomp_release(ctx);
#endif
#endif
}
