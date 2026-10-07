#if   defined(__linux__)
#	include "sandbox/linux.c"
#elif defined(__OpenBSD__)
#	include "sandbox/openbsd.c"
#else
	void sandbox(void) {}
#endif
