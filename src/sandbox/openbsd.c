#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ERR(func) do {perror(func); exit(EXIT_FAILURE);} while (0)

void sandbox(void) {
	unveil("./", "r");
	unveil("./exec/", "rx");
	unveil("./rw/", "rwc");
	unveil(NULL, NULL);

	if (pledge(
	"stdio rpath wpath cpath inet fattr flock unix dns tty prot_exec",
	""
	) != 0)
		ERR("pledge");
}
