#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

static void sandbox_no_new_privs(void)
{
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
		ERR("prctl");
}
