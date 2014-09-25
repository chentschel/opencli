/**
 *	Copyright (C) 2005 Christian Hentschel.
 *
 *	This file is part of Open_cli.
 *
 *	Open_cli is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Open_cli is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with Open_cli; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	Christian Hentschel
 *	chentschel@arnet.com.ar
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "init_modules.h"

#define VERSION "0.1"

static void print_license(void)
{
        printf("OpenCLI %s, Copyright (C) 2005 Christian Hentschel\n"
               "OpenCLI comes with ABSOLUTELY NO WARRANTY; see COPYING.\n"
               "This is free software, and you are welcome to redistribute\n"
               "it under certain conditions; see COPYING for details.\n",
               VERSION);
}

static void init_extensions(void)
{
        extern initcall_t __start_module_init[], __stop_module_init[];
        initcall_t *p;
	int ret;

        for (p = __start_module_init; p < __stop_module_init; p++) {
                ret = (*p)();
		if (ret)
			printf("Initcall %p failed %d\n", p, ret);
	}
}

int main(int argc, char *argv[])
{
	print_license();
	init_extensions();
	
	printf("Extensions loaded.\n");
	
	while (1) {
		printf("system sleeping...\n");
		sleep(5);
	}
	return 0;
}
