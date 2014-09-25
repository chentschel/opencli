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
 
#ifndef __INIT_MODULES_H__
#define __INIT_MODULES_H__

/* init code */
typedef int (*initcall_t)(void);
#define init_call(fn) \
	static initcall_t __initcall_##fn \
	__attribute__((__unused__)) \
	__attribute__((__section__("module_init"))) = &fn	


#endif /* __INIT_MODULES_H__ */
