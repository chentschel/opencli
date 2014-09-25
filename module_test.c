#include <stdio.h>
#include "init_modules.h"

static int init(void)
{
	printf("SOY EL TIMER LOCO!\n");
	
	return 0;
}

init_call(init);
