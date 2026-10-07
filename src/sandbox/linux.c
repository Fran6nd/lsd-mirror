#define _GNU_SOURCE

#ifndef WITHOUT_NO_NEW_PRIVS
#	include "linux_no_new_privs.c"
#endif

#ifndef WITHOUT_UNSHARE
#	include "linux_unshare.c"
#endif

#ifndef WITHOUT_LANDLOCK
#	include "linux_landlock.c"
#endif

#ifndef WITHOUT_SECCOMP
#	include "linux_seccomp.c"
#endif

void sandbox(void)
{
#ifndef WITH_NO_NEW_PRIVS
	sandbox_no_new_privs();
#endif

#ifndef WITH_UNSHARE
	sandbox_unshare();
#endif

#ifndef WITH_LANDLOCK
	sandbox_landlock();
#endif

#ifndef WITH_SECCOMP
	sandbox_seccomp();
#endif
}
