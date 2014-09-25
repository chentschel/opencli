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

#include <pthread.h>

#include <string.h>
#include <stdarg.h>

#include <ctype.h> /* toupper() */

#include <signal.h>

#include <sys/types.h>
#include <arpa/telnet.h>

#include <errno.h>

#include <sys/utsname.h>

#include "lists.h"
#include "init_modules.h"

/* FIXME */

#include "vty.h"
#include "tcp_socket.h"
#include "commands.h"

/* FIXME */

#define LISTENING_PORT		8999
#define STD_BACKLOG		512
#define MAX_CLIENTS		10

#define TERMINATE(ptr)	do { *ptr = '\0';} while(0)
#define M_ZERO(ptr)	do { memset(ptr, 0,sizeof(typeof(*ptr))); } while(0)

extern struct list_head normal_cmd;

/* vty connections 
 */
LIST_HEAD(idl_vtys);
LIST_HEAD(act_vtys);

/* FIXME */

static inline void *v_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		abort();
	
	return ptr;
}


/********************************* H I S T O R Y ************************************/

static inline void hist_write_entry(CL_SOCK *vty, char *cmd)
{
	vty->rl_index = sprintf(vty->rl_buff, cmd);
	vty->rl_ptr = vty->rl_buff + vty->rl_index;
}

static inline void history_clear(struct history *h)
{
	int i = 0;
	
	for(; i < h->len; i++)
		free(h->history[i]);
	
	h->len = h->pos = 0;
}

static char *hist_get_previous(struct history *h)
{
	h->pos++;
	
	if (h->pos >= h->len) {
		h->pos = h->len;
		return NULL;
	}
	return h->history[h->pos];
}

static int history_prev(CL_SOCK *vty)
{
	char *cmd;

	if ((vty->h_ptr)->pos == (vty->h_ptr)->len)
		return -1;
	
	cmd = hist_get_previous(vty->h_ptr);
	if (!cmd)
		cmd = vty->hst_buff;
	
	hist_write_entry(vty, cmd);
	
	return 0;
}

static char *hist_get_next(struct history *h)
{
	if (h->pos <= 0)
		return NULL;
	
	return h->history[--h->pos];
}

/* Takes care of actual command line.*/
static int history_next(CL_SOCK *vty)
{
	char *cmd = hist_get_next(vty->h_ptr);
		
	if (!cmd)
		return -1;

	if ((vty->h_ptr)->pos == (vty->h_ptr)->len - 1)
		strcpy(vty->hst_buff, vty->rl_buff);
		
	hist_write_entry(vty, cmd);

	return 0;
}

static int history_first(CL_SOCK *vty)
{
	return 0;
}

static int history_last(CL_SOCK *vty)
{
	return 0;
}

static inline int is_emptyline(char *cmd)
{
	while (*cmd == ' ')
		cmd++;
	
	return (*cmd) ? 0 : 1;
}

static int history_del(struct history *h, int pos)
{
	register int i;
	
	free(h->history[pos]);
	h->len--;
	
	for(i = pos; i < MAX_HISTORY; i++)
		h->history[i] = h->history[i+1];
	
	return 0;
}

static int history_add(struct history *h, char *cmd)
{
	if (is_emptyline(cmd))
		return -1;
	
	if (h->len > 0 && strcmp(h->history[h->len - 1], cmd) == 0)
		return -1;
		
	if (h->len >= MAX_HISTORY)
		history_del(h, 0);		

	h->history[h->len] = strdup(cmd);
	
	h->len++;
	h->pos = h->len;
	
	return 0;
}

/*************************** V T Y  S P E C I F I C ******************************/

static void vty_start_options(CL_SOCK *vty)
{
	const char options[] = {
		IAC, DO, TELOPT_SGA,
		IAC, DO, TELOPT_ECHO,
		IAC, WILL, TELOPT_ECHO,
		IAC, WILL, TELOPT_SGA,
		IAC, DONT, TELOPT_LINEMODE,
		IAC, DO, TELOPT_NAWS, '\0'
	};
	vty_out(vty, "%s", options);
}

static void init_vty(CL_SOCK *vty)
{
	history_clear(&vty->exec_h);
	history_clear(&vty->conf_h);
	
	vty->w_width = SCR_WIDTH;
	vty->w_height = SCR_HEIGHT;
	
	/* Start with normal = 0 mode. */
	vty->mode = 0;
	vty->privilege = 0;
	
	/* send out telnet vty options */
	vty_start_options(vty);
}

static void reset_vty(CL_SOCK *vty)
{
	vty->conf_h.pos = vty->conf_h.len;
	vty->exec_h.pos = vty->exec_h.len;
	
	vty->h_ptr = (vty->mode == 0) ? 
		 &vty->conf_h : &vty->exec_h;
	
	vty->rl_ptr = vty->rl_buff;
	vty->rl_buff[0] = '\0';
	
	vty->rl_index = 0;
	
	vty->old_buff[0] = '\0';
	vty->old_ptr = vty->rl_ptr;

/*FIXME*/
	vty->flags &= ~FL_ESC_CHAR;
}

static void vty_close(CL_SOCK *vty)
{
	if (vty->fd >= 0)
		close(vty->fd);
	
	/* free history buffers and other dynamic stuff */
	move_to_tail(&vty->conn_list, &idl_vtys);
	
	pthread_exit(EXIT_SUCCESS);
}

/************************ T E L N E T  C O M M A N D S ***************************/

static inline void sb_clear(struct telnet_opt *opt)
{
	opt->sb_ptr = opt->sb_buff;
}

static inline void sb_add(struct telnet_opt *opt, unsigned int c)
{
	if (opt->sb_ptr < opt->sb_buff + sizeof(opt->sb_buff))
		*opt->sb_ptr++ = c;
}

static void hdl_iac_sb(CL_SOCK *vty)
{
	switch (vty->t_opt.sb_buff[0]) {
	
	case TELOPT_NAWS:
		if ((vty->t_opt.sb_ptr - vty->t_opt.sb_buff) < 5)
			break;
		vty->w_width = vty->t_opt.sb_buff[1] << 8;
		vty->w_width |= vty->t_opt.sb_buff[2];
		
		vty->w_height = vty->t_opt.sb_buff[3] << 8;
		vty->w_height |= vty->t_opt.sb_buff[4];
		break;

	/* More subopts here.!!
	 */
	
	}	
}

static int telnet_cmd(CL_SOCK *vty, unsigned int c)
{
	struct telnet_opt *opt = &vty->t_opt;
	
	switch (opt->state) {
	
	case 0: 
		if (c == IAC)
			opt->state = IAC;
		else
			return 0;
		break;

	case IAC:
		switch (c) {
		case IAC:
			return 0;
		case SB:
			opt->state = SB;
			sb_clear(opt);
			return 1;
		}
		if (TELCMD_OK(c))
			break;
		
		opt->state = 0;
		break;
	
	case SB:
		if (c == IAC)
   			opt->state = SE;
		else
	    		sb_add(opt, c);
		break;
		
	case SE:
		if (c != SE) {
			opt->state = SB;
			sb_add(opt, c);
		} else {
			hdl_iac_sb(vty);
	      		opt->state = 0;
		}
		break;
	}
	return 1;
}

/******************************* V T Y  R E A D *********************************/

static inline int s_buff_count(CL_SOCK *vty)
{
	return vty->s_count;
}

static inline int b_getchar(CL_SOCK *vty)
{
	vty->s_count--;
	
	return *(vty->s_ptr++) & 0xFF;
}

static int n_getchar(CL_SOCK *vty)
{	
	if (!s_buff_count(vty)) {
		vty->s_count = sk_recv(vty->fd, vty->s_buff, 
						sizeof(vty->s_buff), 0);
		vty->s_ptr = vty->s_buff;
		
		if (vty->s_count <= 0)
			vty_close(vty);
	}
	return b_getchar(vty);
}
			
/* Recv byte from net, filtering special telnet chars.. */
static int telnet_getchar(CL_SOCK *vty)
{
	register int c;
	
	do {
		c = n_getchar(vty);
		
	} while (telnet_cmd(vty, c) || c == '\n' || c == '\0');
	
	return c;
}

/***************************** V T Y  W R I T E *******************************/


enum MORE_OPTS { MORE_LINE, MORE_PAGE, MORE_ABORT };

void automore_on(CL_SOCK *vty)
{
	vty->paged_output = 1;
}

void automore_off(CL_SOCK *vty)
{
	vty->paged_output = 0;
	vty->ln_count = 0;
}

static int ask_automore(CL_SOCK *vty)
{
	int c;
        
	if (vty->ln_count == 0)
        	return MORE_PAGE;
	
	while (1) {
		sk_send_all(vty->fd, " --More-- ", 10);
		
		c = telnet_getchar(vty);
		
		sk_send_all(vty->fd, "\b\b\b\b\b\b\b\b\b        \b\b\b\b\b\b\b\b\b", 26);
		
		switch (c) {
		
		case '?':
			sk_send_all(vty->fd, "Press RETURN for another line, "
				"SPACE for another page, anything else to quit\r\n", 78);
			break;
		case ' ':/* Space */
		case 'Y':/* Yes */
		case 'y':
			vty->ln_count = 0;
			return MORE_PAGE;	
		case '\r':
			vty->ln_count = vty->w_height - 2;
			return MORE_LINE;	
		default:
			automore_off(vty);
			return MORE_ABORT;
		}
	}
}	

static int automore_output(CL_SOCK *vty)
{
	register char *ptr = vty->w_buff, *aux = ptr;
	
	while (*aux) {
		if (*aux == '\n')
			vty->ln_count++;
		
		aux++;
		
		if (vty->ln_count == vty->w_height - 1) {
			
			sk_send_all(vty->fd, ptr, aux - ptr);
			
			/* Reload start ptr..*/
			ptr = aux;	
			if (ask_automore(vty) == MORE_ABORT)
				return -1;
		}
	}
	/* Send the rest, if any.. */
	return sk_send_all(vty->fd, ptr, aux - ptr);
}

/* FIXME : implementar buffer dinamico para el send?.. o controlar bcount. 
 */
int vty_out(CL_SOCK *vty, const char *fmt, ...)
{
        int bcount;
	
	va_list args;
        va_start(args, fmt);

        bcount = vsnprintf(vty->w_buff, sizeof(vty->w_buff), fmt, args);

        va_end(args);
	
	if (bcount < 0)
                perror("sprintf()");
        else {
		if (bcount >= sizeof(vty->w_buff))
			printf("Warning, string truncated. %s\n", vty->w_buff);
	
		if (vty->paged_output) {
			automore_output(vty);
		} else
			sk_send_all(vty->fd, vty->w_buff, bcount);
	}
        return bcount;
}

/**************************** C U R S O R  D I S P L A Y *******************************/

/**
 * Move cursor, while old and new posisions not equal.
 */
static inline void rl_movecursor(char *to, char **from, char **buff)
{	
	if (to == *from)
		return;
	
	while (to < *from) {
		*(*buff)++ = '\b';
		(*from)--;
	}
	while (to > *from)
		*(*buff)++ = *(*from)++;
}

static inline void rl_update(CL_SOCK *vty)
{
	register char *new = vty->rl_buff, *old = vty->old_buff;
	char *to = vty->rl_ptr, *from = vty->old_ptr;
	
	char *buff = vty->w_buff;
						
	for (; *new && *old; new++, old++) {
		if (*new != *old) {
			to = new;
			break;
		}
	}

	rl_movecursor(to, &from, &buff);

	for (; *new && *old; new++, old++);
	
	if (*new || *old) {
	
		to = new;

		for (; *new; new++, to++);

		rl_movecursor(to, &from, &buff);

		for (; *old; old++, from++)
			*buff++ = ' ';
	}
	
	rl_movecursor(vty->rl_ptr, &from, &buff);


	/* Send changes to client..
	 */
	sk_send_all(vty->fd, vty->w_buff, buff - vty->w_buff);
	
	strcpy(vty->old_buff, vty->rl_buff);
	vty->old_ptr = vty->rl_ptr;
}

/******************************** P R I M I T I V E S ***********************************/

static int save_char(CL_SOCK *vty, char c)
{
	char *ptr;

	if (vty->rl_index >= MAX_CMDLINE - 1)
    		return -1;
	
	vty->rl_index++;
	ptr = vty->rl_buff + vty->rl_index;
	
	while (ptr != vty->rl_ptr){
		*ptr = *(ptr - 1);
		ptr--;
	}
	*vty->rl_ptr++ = c;
	
	return 0;
}

static inline int clear_char(CL_SOCK *vty)
{
	char *ptr;
	
	if (vty->rl_ptr == vty->rl_buff)
		return -1;

	vty->rl_index--;
	vty->rl_ptr--;
	
	ptr = vty->rl_ptr;
	
	while (*ptr != '\0') {
		*ptr = *(ptr + 1);
		ptr++;
	}
	return 0;
}

/************************************ D E L E T E ****************************************/

static int bkspace_hdl(CL_SOCK *vty)
{
	return clear_char(vty);
}

static int clear_word(CL_SOCK *vty)		
{	
	while (*(vty->rl_ptr - 1) == ' ' && !clear_char(vty));
	while (*(vty->rl_ptr - 1) != ' ' && !clear_char(vty));

	return 0;
}

static int clear_line(CL_SOCK *vty)
{
	while (clear_char(vty) != -1);
	
	return 0;
}

static int delete_word(CL_SOCK *vty)
{
	return 0;
}

/*********************************** M O V E M E N T *************************************/

static inline int is_delimiter(char c)
{
	return (c == ' ' || c == '-' || c == '.' || c == '/' || c == '@');
}

static int move_to_bol(CL_SOCK *vty)
{
	while (vty->rl_ptr > vty->rl_buff)
		vty->rl_ptr--;
	
	return 0;
}

static int move_to_eol(CL_SOCK *vty)
{
	while (*vty->rl_ptr != '\0')
		vty->rl_ptr++;
	
	return 0;
}

static int backward_char(CL_SOCK *vty)
{
	if (vty->rl_ptr > vty->rl_buff)
		vty->rl_ptr--;	
	
	return 0;
}

static int forward_char(CL_SOCK *vty)
{
	if (*vty->rl_ptr != '\0')
		vty->rl_ptr++;

	return 0;
}

static int forward_word(CL_SOCK *vty)
{
	while (*vty->rl_ptr != '\0' && is_delimiter(*vty->rl_ptr))
		vty->rl_ptr++;
	
	while (*vty->rl_ptr != '\0' && !is_delimiter(*vty->rl_ptr))
		vty->rl_ptr++;
	
	return 0;
}

static int backward_word(CL_SOCK *vty)
{
	while (vty->rl_ptr > vty->rl_buff && is_delimiter(*(vty->rl_ptr - 1)))
		vty->rl_ptr--;

	while (vty->rl_ptr > vty->rl_buff && !is_delimiter(*(vty->rl_ptr - 1)))
		vty->rl_ptr--;
		
	return 0;
}

/********************** M I C E L L A N E O U S  E D I T I N G *************************/

static int capitalize_word(CL_SOCK *vty)
{
	while (*vty->rl_ptr != '\0' && is_delimiter(*vty->rl_ptr))
		vty->rl_ptr++;

	if (*vty->rl_ptr == '\0')
		return 0;
	
	*vty->rl_ptr = toupper(*vty->rl_ptr);
	vty->rl_ptr++;
	
	while (*vty->rl_ptr != '\0' && !is_delimiter(*vty->rl_ptr)) {
		*vty->rl_ptr = tolower(*vty->rl_ptr);
		vty->rl_ptr++;
	}
	return 0;
}

static int tolower_word(CL_SOCK *vty)
{
	while (*vty->rl_ptr != '\0' && is_delimiter(*vty->rl_ptr))
		vty->rl_ptr++;

	while (*vty->rl_ptr != '\0' && !is_delimiter(*vty->rl_ptr)) {
		*vty->rl_ptr = tolower(*vty->rl_ptr);
		vty->rl_ptr++;
	}
	return 0;
}

static int touppper_word(CL_SOCK *vty)
{
	while (*vty->rl_ptr != '\0' && is_delimiter(*vty->rl_ptr))
		vty->rl_ptr++;

	while (*vty->rl_ptr != '\0' && !is_delimiter(*vty->rl_ptr)) {
		*vty->rl_ptr = toupper(*vty->rl_ptr);
		vty->rl_ptr++;
	}
	return 0;				
}

static int transpose_chars(CL_SOCK *vty)
{
	return 0;
}

/***************************************************************************************/

static int reprint_line(CL_SOCK *vty)
{
/*FIXME*/
	vty_out(vty, "\r\n%s%s", 
		cmd_get_hostname(), 
		cmd_mode_prompt(vty));
	
	vty->old_buff[0] = '\0';
	vty->old_ptr = vty->rl_buff;
	
	return 0;	
}

static int escape_char_hdl(CL_SOCK *vty)
{
	vty->flags |= FL_ESC_CHAR;
	
	return 0;
}

static int quoted_insert(CL_SOCK *vty)
{
	vty->flags |= FL_ESC_TERM;
	
	return 0;
}

static int spec_chars_hdl(CL_SOCK *vty, char c)
{
        switch (c) {

	case CTRL_A:	return move_to_bol(vty);
	case CTRL_E:	return move_to_eol(vty);
	
	case CTRL_B:	return backward_char(vty);
	case CTRL_F:	return forward_char(vty);

	case DEL:
	case BS:	return bkspace_hdl(vty);
	
	case CTRL_R:
	case CTRL_L:	return reprint_line(vty);
	
	case CTRL_N:	return history_next(vty);
	case CTRL_P:	return history_prev(vty);
	
	case CTRL_T:	return transpose_chars(vty);
	
	case ESC:	return escape_char_hdl(vty);

	case CTRL_X:
	case CTRL_U:	return clear_line(vty);
	case CTRL_W:	return clear_word(vty);
	
	default:	break;
	}
	return -1;
}

static int esc_chars_hdl(CL_SOCK *vty, char c)
{
	switch (c) {
	
	case '<':	return history_first(vty);
	case '>':	return history_last(vty);
	
	case 'Q':
	case 'q':	return quoted_insert(vty);
	
	case 'A':	return history_next(vty);
	case 'B':	return history_prev(vty);
	
	case 'C':	return forward_char(vty);
	case 'D':	return backward_char(vty);
	
	case 'b':	return backward_word(vty);
	case 'f':	return forward_word(vty);
	
	case 'c':	return capitalize_word(vty);
	case 'd':	return delete_word(vty);

	case 'l':	return tolower_word(vty);
	case 'u':	return touppper_word(vty);

	case DEL:	return clear_line(vty);
	default:	break;
	}
	return -1;
}

/*****************************************************************************************/

static void cmd_print_matched(CL_SOCK *vty, char *cmd, struct list_head *cmdLst)
{
	int p_count = 5, m_count = 0;
	struct list_head *tmp;
		
	iterate(tmp, cmdLst) {

		CLI_CMD *n = container_of(tmp, CLI_CMD, parent_lst);
			
		if ((vty->privilege < n->privilege)
		    || !cmd_is_completion(cmd, n->cmda)) {
			continue;	
		}
		m_count++;
		
		vty_out(vty, "%-.15s%s", n->cmda, (p_count) ? "\t":"\r\n");
		
		if (!--p_count)
			p_count = 5;
	}
	if (!m_count)
		vty_out(vty, "%% Unrecognized command.");

	vty_out(vty, "\r\n");
}

static void cmd_print_args(CL_SOCK *vty, struct list_head *cmdLst)
{
	struct list_head *tmp;
	
	iterate(tmp, cmdLst) {
		CLI_CMD *n = container_of(tmp, CLI_CMD, parent_lst);
		
		if (vty->privilege >= n->privilege)
			vty_out(vty, "  %-17s%s\r\n", n->cmda, n->summary);
	}

	/* Let's print command args here..
	 * 
	 * print_cmd_args();
	 *
	 * vty_out(conn, "  <cr>\r\n");
	 */
}

static void cmd_err_hld(CL_SOCK *vty, int act, int err)
{
	switch (err) {

	case CMD_INCOMPLETE:
		vty_out(vty, "%% Incomplete command.\r\n\r\n");
		break;
	
	case CMD_INVALID:
		if (act == HELP || act == C_HELP)
			vty_out(vty, "%% Unrecognized command.\r\n");
		else
			vty_out(vty, "%*c\r\n%% Invalid input detected"
				" at '^' marker.\r\n\r\n", 10 + strlen(vty->rl_buff), '^');
		break;
	
	case CMD_AMBIGUOUS:
		vty_out(vty, "%% Ambiguous command:  \"%s\"\r\n", vty->rl_buff);
	}
}

static inline void cmd_execute(CL_SOCK *vty, CLI_CMD *match)
{
	if (!match->handler) {
		vty_out(vty, "%% Type \"%s ?\" for "
				"a list of subcommands\r\n", match->cmda);
	} else
		match->handler(vty);
}

static inline void cmd_complete(CL_SOCK *vty, char *match)
{
	while ((*(vty->rl_ptr - 1) != ' ')
	    && (--(vty->rl_ptr) != vty->rl_buff));

	vty->rl_ptr += sprintf(vty->rl_ptr, "%s ", match);
	vty->rl_index = strlen(vty->rl_buff);
}

static void cmd_action_onbrk(CL_SOCK *vty, int act)
{
	struct list_head *cmdPtr = cmd_get_cmdroot(vty);
	int ret = CMD_FOUND;
	CLI_CMD	*match;
	
	if (vty->argc == 1 && act == C_HELP)
		return cmd_print_matched(vty, vty->argv[0], cmdPtr);

	if (vty->argc) {
		ret = cmd_finder(vty->argv, cmdPtr, &match, act, vty->privilege);
		cmdPtr = &match->child_lst;
	}
	if (ret == CMD_FOUND) {
		switch (act) {
		case C_HELP:
			cmd_print_matched(vty, vty->argv[vty->argc - 1], cmdPtr);
			break;
		case HELP:
			cmd_print_args(vty, cmdPtr);
			break;
		case FIND:	
			cmd_execute(vty, match);
			break;
		case COMPLETE:
			cmd_complete(vty, match->cmda);
		}
	
	} else if (act != COMPLETE)
		cmd_err_hld(vty, act, ret);
}

static inline int has_input(CL_SOCK *vty)
{
	return (vty->rl_ptr > vty->rl_buff && *(vty->rl_ptr - 1) != ' ');
}

static void exec_hdl(CL_SOCK *vty)
{
	if (vty->argc) {
		history_add(vty->h_ptr, vty->rl_buff);
		cmd_action_onbrk(vty, FIND);
	}
	reset_vty(vty);
}

static void help_hdl(CL_SOCK *vty)
{
	if (vty->argc && has_input(vty))
		cmd_action_onbrk(vty, C_HELP);
	else 
		cmd_action_onbrk(vty, HELP);
}

static void tabl_hdl(CL_SOCK *vty)
{
	if (vty->argc && has_input(vty))
		cmd_action_onbrk(vty, COMPLETE);
}

/*************************** R E A D L I N E  F U N C T I O N *******************************/

int readline(CL_SOCK *vty, const char *prompt, const char *breakset, int doecho)
{
	register int c;
	
	vty_out(vty, prompt);
	rl_update(vty);
	
	while (1) {
		
		c = telnet_getchar(vty);

		if (vty->flags & FL_ESC_CHAR) {
			
			/* Special case, c is an arrow..
			 */
			if (c == 'O' || c == '[')
				continue;

			if (esc_chars_hdl(vty, c) < 0)
				vty_out(vty, "%c", BELL);

			vty->flags &= ~FL_ESC_CHAR;
		
		} else if (vty->flags & FL_ESC_TERM) {
			if (c != '\r' && c < 127) {
				if (save_char(vty, c) < 0)
					vty_out(vty, "%c", BELL);
			}		
			vty->flags &= ~FL_ESC_TERM;
		
		} else if (strchr(breakset, c)) {
			break;

		} else if (c < SP || c == DEL) {
			if (c == CTRL_C) {
				reset_vty(vty);
				return -1;
			}
			if (spec_chars_hdl(vty, c) < 0)
				vty_out(vty, "%c", BELL);
		} else {
			if (save_char(vty, c) < 0)
				vty_out(vty, "%c", BELL);
		}
		if (doecho)
			rl_update(vty);
	}
	return c;	
}

/*************************  M A I N  C O N F I G U R A T I O N  ***********************/

struct host_config {
	
	int encrypt;
	char *username;
	char *passwd, *ena_passwd;
	
	char *hostname;
	char *motd;
};

struct host_config global_conf = { 
	0, "cisco", "cisco", "cisco", "open_cli", 
	"This should be the initial banner.." 
};


const char *cmd_get_hostname(void)
{
	const char *hostname = global_conf.hostname;
	struct utsname n;
	
	if (!hostname) {
		uname(&n);
		hostname = n.nodename;
	}
	return hostname;
}

/***************************  L O G U I N  S T U F F  ********************************/

void cmd_send_banner(CL_SOCK *vty)
{
	if (global_conf.motd)
		vty_out(vty, "\r\n%s\r\n", global_conf.motd);
}

int user_lookup(char *username)
{
	if (strcmp(username, global_conf.username))
		return -1;
	
	return 0;
}

int ask_password(CL_SOCK *vty)
{
	const char *passwd;
	int fail;
	
	reset_vty(vty);
	readline(vty, "\r\nPassword: ", "\r", 0);
  
	if (vty->mode == 0) {
		passwd = global_conf.passwd;
	} else
		passwd = global_conf.ena_passwd;

	if (global_conf.encrypt)
		fail = strcmp((const char *)crypt(vty->rl_buff, passwd), passwd);
	else
		fail = strcmp(vty->rl_buff, passwd);
	
	return (fail) ? -1 : 0;
}

int login(CL_SOCK *vty)
{
	int failed = 0;
	
	vty_out(vty, "\r\n\r\nUser Access Verification\r\n");

	while (1) {
		reset_vty(vty);

		readline(vty, "\r\nUsername: ", "\r", 1);
		
		if (!*vty->rl_buff)
			continue;
			
		if (user_lookup(vty->rl_buff) < 0 || ask_password(vty) < 0) {
			vty_out(vty, "\r\n%% Login invalid\r\n");
			
			if (failed++ == 2)
				return -1;
		} else
			break;
	}
	vty_out(vty, "\r\n");
	
	return 0;
}

void *vty_main(void *data)
{
	CL_SOCK *vty;
	
	pthread_detach(pthread_self());
	
	vty = (CL_SOCK *)data;
	
	init_vty(vty);
	cmd_send_banner(vty);
	
	if (login(vty) < 0)
		vty_close(vty);

	reset_vty(vty);

	while (1) {
		char vty_prompt[128];
		int c;
		
		sprintf(vty_prompt, "%s%s", 
				cmd_get_hostname(), 
				cmd_mode_prompt(vty));
		
		c = readline(vty, vty_prompt, "\r\t?", 1);
	
		/* At break char, we've to actualize */
		vty->rl_ptr = vty->rl_buff + vty->rl_index;
	
		vty_out(vty, "%s\r\n", (c == '?') ? "?" : "");
	
		vty->argc = getline(vty->rl_buff, vty->argv);
	
		switch (c) {
			case '\r':	exec_hdl(vty); break;
			case '\t':	tabl_hdl(vty); break;
			case '?':	help_hdl(vty); break;
		}
		while (vty->argc--)
			free(vty->argv[vty->argc]);
		
		vty->old_ptr = vty->rl_buff;
	}
}

static int new_vty(int nfd, struct sockaddr_in *addr)
{	
	pthread_t vty_tid;
	
	if (list_empty(&idl_vtys)) {
		printf("Max allowed vty reached.\n");
		return -1;
	}
	
	CL_SOCK *vty = container_of(idl_vtys.next, CL_SOCK, conn_list);	
	
	list_del(&vty->conn_list);
	
	M_ZERO(vty);

	vty->fd = nfd;
	vty->address = addr->sin_addr.s_addr;

	if (pthread_create(&vty_tid, NULL, vty_main, vty)) {
		perror("pthread_create()");
		
		list_add(&vty->conn_list, &idl_vtys);
		return -1;
	}
	
	list_add(&vty->conn_list, &act_vtys);
	
	return 0;
}

static void alloc_vtys(int vty_num)
{
	int i;
	
	for (i = 0; i < vty_num; i++) {
		CL_SOCK *n = (CL_SOCK *)v_malloc(sizeof(CL_SOCK));
			
		list_add(&n->conn_list, &idl_vtys);
	}	
}

static inline void vty_acceptor(int sfd)
{
	struct sockaddr_in addr;
	int nfd;
	
	while ((nfd = sk_accept(sfd, &addr)) != -1) {
		if (new_vty(nfd, &addr) < 0) {
			printf("Could not initialize vty.\n");
			close(nfd);
		}	
	}
}

void register_signal(int signum, void (*sig_handler)(int))
{
	struct sigaction sa;	

	M_ZERO(&sa);
	sa.sa_handler = sig_handler;

	if (sigaction(signum, &sa, NULL) == -1){
		perror("sigaction()");
		abort();
	}
}

void *vty_thread(void)
{
	int sfd, nfd;
	int flags = 1;
	struct linger ling = { 0, 0 };
	struct sockaddr_in addr;
	
	register_signal(SIGPIPE, SIG_IGN);
	
	alloc_vtys(MAX_CLIENTS);
	
	if ((sfd = sk_socket(PF_INET, SOCK_STREAM, 0)) == -1)
		abort();

	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
	setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
	setsockopt(sfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));

	if (sk_set_server(sfd, LISTENING_PORT, STD_BACKLOG) == -1)
		abort();

	while ((nfd = sk_accept(sfd, &addr)) != -1) {
		if (new_vty(nfd, &addr) < 0) {
			printf("Could not initialize vty.\n");
			close(nfd);
		}	
	}
}

static int vty_init(void)
{
	pthread_t	t_vty;
	pthread_attr_t	t_attr;
	
	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
	
	if (pthread_create(&t_vty, &t_attr, vty_thread, NULL)) {
		perror("pthread_create()");
		return -1;
	}
	return 0;
}

init_call(vty_init);
