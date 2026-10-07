#include <features.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <stddef.h>
#include <sys/syscall.h>

/* TODO: fire up QEMU */
#if   defined(__i386__)
#	define ARCH AUDIT_ARCH_I386
#elif defined(__x86_64__)
#	define ARCH AUDIT_ARCH_X86_64
#elif defined(__arm__)
#	define ARCH AUDIT_ARCH_ARM
#else
#	error unknown seccomp arch
#endif

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

#define allow(nr) BPF_JUMP( \
	BPF_JMP | BPF_JEQ | BPF_K, \
	nr, \
	SYSCALLS - __COUNTER__, \
	0 \
)

#define SYSCALLS_BASE 51
#define SYSCALLS_GLIBC 5

#ifdef __GLIBC__
#define SYSCALLS SYSCALLS_BASE + SYSCALLS_GLIBC
#else
#define SYSCALLS SYSCALLS_BASE
#endif

static void sandbox_seccomp(void)
{
	struct sock_filter filter[] = {
		BPF_STMT(
			BPF_LD | BPF_W | BPF_ABS,
			offsetof(struct seccomp_data, arch)
		),

		BPF_JUMP(
			BPF_JMP | BPF_JEQ | BPF_K,
			ARCH,
			0,
			1 + SYSCALLS /* kill */
		),

		BPF_STMT(
			BPF_LD | BPF_W | BPF_ABS,
			offsetof(struct seccomp_data, nr)
		),

		allow(SYS_read),            /*   0 */
		allow(SYS_write),           /*   1 */
		allow(SYS_open),            /*   2 */
		allow(SYS_close),           /*   3 */
		allow(SYS_stat),            /*   4 */
		allow(SYS_fstat),           /*   5 */
		allow(SYS_lstat),           /*   6 */
		allow(SYS_poll),            /*   7 */
		allow(SYS_lseek),           /*   8 */
		allow(SYS_mmap),            /*   9 */
		allow(SYS_mprotect),        /*  10 */
		allow(SYS_munmap),          /*  11 */
		allow(SYS_brk),             /*  12 */
		allow(SYS_rt_sigaction),    /*  13 */
		allow(SYS_rt_sigprocmask),  /*  14 */
		allow(SYS_rt_sigreturn),    /*  15 */
		allow(SYS_ioctl),           /*  16 */
		allow(SYS_pread64),         /*  17 */
		allow(SYS_pwrite64),        /*  18 */
		allow(SYS_readv),           /*  19 */
		allow(SYS_writev),          /*  20 */
		allow(SYS_mremap),          /*  25 */
		allow(SYS_setitimer),       /*  38 */
		allow(SYS_getpid),          /*  39 */
		allow(SYS_socket),          /*  41 */
		allow(SYS_connect),         /*  42 */
		allow(SYS_accept),          /*  43 */
		allow(SYS_sendto),          /*  44 */
		allow(SYS_recvfrom),        /*  45 */
		allow(SYS_sendmsg),         /*  46 */
		allow(SYS_recvmsg),         /*  47 */
		allow(SYS_bind),            /*  49 */
		allow(SYS_listen),          /*  50 */
		allow(SYS_getsockname),     /*  51 */
		allow(SYS_getpeername),     /*  52 */
		allow(SYS_setsockopt),      /*  54 */
#ifdef __GLIBC__
		allow(SYS_uname),           /*  63 */
#endif
		allow(SYS_fcntl),           /*  72 */
		allow(SYS_fsync),           /*  74 */
		allow(SYS_fdatasync),       /*  75 */
		allow(SYS_ftruncate),       /*  77 */
		allow(SYS_getcwd),          /*  79 */
		allow(SYS_rename),          /*  82 */
		allow(SYS_unlink),          /*  87 */
		allow(SYS_fchmod),          /*  91 */
		allow(SYS_geteuid),         /* 107 */
#ifdef __GLIBC__
		allow(SYS_futex),           /* 202 */
#endif
		allow(SYS_getdents64),      /* 217 */
		allow(SYS_restart_syscall), /* 219 */
		allow(SYS_exit_group),      /* 231 */
#ifdef __GLIBC__
		allow(SYS_openat),          /* 257 */
		allow(SYS_newfstatat),      /* 262 */
		allow(SYS_sendmmsg),        /* 307 */
#endif
		allow(SYS_getrandom),       /* 318 */
		allow(SYS_membarrier),      /* 324 */
		allow(SYS_pwritev2),        /* 328 */

		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
	};

	if (SYSCALLS != sizeof(filter)/sizeof(*filter) - 3 - 2)
		abort();

	struct sock_fprog prog;
	prog.len = sizeof(filter)/sizeof(*filter);
	prog.filter = filter;

	if (syscall(
		SYS_seccomp,
		SECCOMP_SET_MODE_FILTER,
		SECCOMP_FILTER_FLAG_TSYNC,
		&prog
	) != 0)
		ERR("seccomp");
}
