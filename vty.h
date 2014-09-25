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


#ifndef __VTY_H__
#define __VTY_H__


#define CTRL_A	0x01
#define	CTRL_B	0x02
#define	CTRL_C	0x03
#define	CTRL_D	0x04
#define	CTRL_E	0x05
#define	CTRL_F	0x06

#define BELL	0x07
#define	BS	0x08
#define HT	0x09
#define LF	0x0A
#define VT	0x0B

#define	CTRL_L	0x0C

#define CR	0x0D

#define	CTRL_N	0x0E
#define	CTRL_P	0x10
#define CTRL_Q	0x11
#define CTRL_R	0x12
#define CTRL_S	0x13
#define	CTRL_T	0x14
#define	CTRL_U	0x15
#define CTRL_V	0x16
#define CTRL_W	0x17
#define	CTRL_X	0x18
#define CTRL_Y	0x19
#define CTRL_Z	0x1A

#define	ESC	0x1B

#define FS	0x1C
#define GS	0x1D
#define RS	0x1E
#define US	0x1F
#define SP	0x20

#define DEL	0x7F


#define SCR_WIDTH	80
#define SCR_HEIGHT	25

#define MAX_WBUFFER	1024
#define MAX_RBUFFER	128
#define MAX_CMDLINE 	128
#define MAX_CMDARGS	10

#define MAX_HISTORY	10


/* VTY flags for editline stuff */
#define FL_ESC_TERM	0x01
#define FL_ESC_CHAR	0x02

struct history {
	char	*history[MAX_HISTORY];
	int	len, pos;
};

struct telnet_opt {
	char sb_buff[16], *sb_ptr;
	int state;
};

typedef struct CL_SOCK_ {

	/* History support */
	struct history exec_h, conf_h;
	struct history *h_ptr;
	
	char hst_buff[MAX_CMDLINE];
	
	/* Readline edit support */
	char rl_buff[MAX_CMDLINE];
	char old_buff[MAX_CMDLINE];
	
	/* IO socket buffers */
	char w_buff[MAX_WBUFFER];
	char s_buff[MAX_RBUFFER];
	
	int rl_index, s_count;
	char *rl_ptr, *old_ptr, *s_ptr;
	
	/* Parsed line should go here */
	char *argv[MAX_CMDARGS];
	int argc;
	
	/* VTY flags..*/
	int flags;
	
	/* VTY mode - NORMAL, EXEC, etc */
	int mode;
	
	/* VTY mode - 1 to 15 */
	int privilege;
	
	/* VTY window limits..*/
	int w_width, w_height;
	
	/* Paged output vars..*/
	int paged_output;
	int ln_count;
	
	/* Telnet options */
	struct telnet_opt t_opt;

	u_int32_t address;
	int fd;
		
	struct list_head conn_list;

} CL_SOCK;


int vty_out(CL_SOCK *vty, const char *fmt, ...);

extern __inline__ void automore_on(CL_SOCK *vty);
extern __inline__ void automore_off(CL_SOCK *vty);

#endif /*__VTY_H__*/
