#include <stdio.h>
#include "init_modules.h"

static void run_inits(void)
{
	/* Linker magic creates these to delineate section. */
	extern initcall_t __start_init_call[], __stop_init_call[];
	initcall_t *p;

	for (p = __start_init_call; p < __stop_init_call; p++)
		(*p)();
}

int main(int argc, char **argv)
{
	run_inits();
	return 0;
}
