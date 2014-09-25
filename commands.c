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
#include <string.h>
 
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/sendfile.h>

#include "lists.h"

#include "vty.h"
#include "commands.h"
#include "init_modules.h"

static inline void *v_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		abort();
	
	return ptr;
}

LIST_HEAD(exec_mode);
LIST_HEAD(config_mode);
LIST_HEAD(interf_mode);

typedef enum VTY_MODE_ { NORMAL, ENABLE, CONFIG, INTERF } VTY_MODE;

#define MIN_MODE	NORMAL
#define MAX_MODE	INTERF

struct command_nodes {
	
	VTY_MODE mode;
	struct list_head *cmdPtr;
	const char *prompt;
	
} cmd_nodes[] =
{
	{ NORMAL, &exec_mode, ">" }, 
	{ ENABLE, &exec_mode, "#" },
	{ CONFIG, &config_mode, "(config)#" },
	{ INTERF, &interf_mode, "(config-if)#" }
};


struct list_head *cmd_get_cmdroot(CL_SOCK *vty)
{
	return cmd_nodes[vty->mode].cmdPtr;
}

const char *cmd_mode_prompt(CL_SOCK *vty)
{
	return cmd_nodes[vty->mode].prompt;
}

static int cmd_set_vtymode(CL_SOCK *vty, VTY_MODE newmode)
{
	if (newmode >= MIN_MODE && newmode <= MAX_MODE)
		vty->mode = newmode;
	else
		printf("Error: %d is not a valid mode.\n", newmode);
	
	return 0;
}

void cmd_register(struct list_head *listPtr, CLI_CMD *cmd)
{
	struct list_head *tmp;
	
	iterate(tmp, listPtr) {
		CLI_CMD *c = container_of(tmp, CLI_CMD, parent_lst);
		
		if (strcmp(cmd->cmda, c->cmda) < 0)
			break;
	}
	
	/* Init before list_add. */
	INIT_LIST_HEAD(&cmd->child_lst);
	INIT_LIST_HEAD(&cmd->parent_lst);
	
	list_add_tail(&cmd->parent_lst, tmp);
}

char *cmd_is_completion(char *cmd, char *real_cmd)
{
	while (*cmd && *real_cmd && (*cmd == *real_cmd))
		cmd++, real_cmd++;

	return (!*cmd) ? real_cmd : NULL;
}

int cmd_finder(char **argv, struct list_head *cmd_lst, 
		CLI_CMD **m_match, int act, int vty_privilege)
{
	struct list_head *tmp;
	int i = 0, m_count;
	
	int adjuster = (act == C_HELP) ? 1 : 0;
	
	do {
		*m_match = NULL; m_count = 0;
					
		iterate(tmp, cmd_lst) {
			
			CLI_CMD *n = container_of(tmp, CLI_CMD, parent_lst);

			if ((vty_privilege < n->privilege)
			    || !cmd_is_completion(argv[i], n->cmda)) {
				continue;
			}
			if (m_count++)
				return CMD_AMBIGUOUS;
			*m_match = n;
		}
		if (!*m_match)
			return CMD_INVALID;

		if ((act == FIND) && ((*m_match)->final))
			return (argv[i+1]) ? CMD_FOUND : CMD_INCOMPLETE;
		
		cmd_lst = &(*m_match)->child_lst;
	
	} while (argv[++i + adjuster]);

	return CMD_FOUND;
}

/**********************  B A S I C  S H O W  C O M M A N D S  *********************/

CMD_DEFINE(cmd_show, "show", 
	"Show running system information", 0, 0, NULL);

CMD_DEFINE_FN(show_run, "running-config", 
	"Current operating configuration", 0, 0)
{
	struct stat f_stat;
	off_t offset = 0;
	
	int conf_fd = open("./config.txt", O_RDONLY);

	if (conf_fd < 0 || stat("./config.txt", &f_stat) < 0)
		return vty_out(vty, "Error loading config file.\r\n");
	
	return sendfile(vty->fd, conf_fd, &offset, f_stat.st_size);
}

CMD_DEFINE(show_run_interface, "interface", 
	"Show interface configuration", 0, 0, NULL);

CMD_DEFINE_FN(show_ip, "ip", "IP information", 1, 0)
{
	return vty_out(vty, "Show IP information handler\r\n");
}

CMD_DEFINE_FN(show_interfaces, "interfaces", 
	"Interface status and configuration", 1, 0)
{
	vty_out(vty, "Show Interfaces handler\r\n");

	automore_on(vty);
	
	if_show_interface(vty, vty->argv[2]);
	
	automore_off(vty);
	
	return 0;	
}

CMD_DEFINE(cmd_clear, "clear", 
	"Reset functions", 0, 0, NULL);

CMD_DEFINE_FN(clr_counters, "counters", 
	"Clear counters on one or all interfaces", 0, 0)
{
	vty_out(vty, "Clear counters handler\r\n");
	
	if_clear_counters();
	
	return 0;	
}

CMD_DEFINE_FN(show_debugging, "debugging", 
	"State of each debugging option", 0, 0)
{
	return vty_out(vty, "Show debugging handler\r\n");
}

CMD_DEFINE_FN(show_history, "history", 
	"Display the session command history", 0, 0)
{
	return vty_out(vty, "show history handler\r\n");
}

CMD_DEFINE_FN(show_clock, "clock", 
	"Display the system clock", 0, 0)
{
	struct timeval now;
        struct tm now_tm;
	
	char buffer[100];
	
	gettimeofday(&now, NULL);
	strftime(buffer, 100, ".%T %h %d %Y\r\n", 
			localtime_r(&now.tv_sec, &now_tm));
	
	return vty_out(vty, buffer);
}

/* FIXME */
extern struct list_head act_vtys;

CMD_DEFINE_FN(show_users, "users",
	"Display information about terminal lines", 0, 0)
{
	struct list_head *lnk;
	
	iterate(lnk, &act_vtys) {
		CL_SOCK *n = container_of(lnk, CL_SOCK, conn_list);
		
		vty_out(vty, "vty->address: %u - vty->fd: %u\r\n", 
			n->address, n->fd);
	}
	return 0;
}

/*************************  D E B U G  C O M M A N D S  ************************/

CMD_DEFINE(cmd_debug, "debug", 
	"Debugging functions (see also 'undebug')", 0, 0, NULL);

CMD_DEFINE_FN(debug_h225, "h225", 
	"H.225 Library Debugging", 1, 0)
{
	return vty_out(vty, "debug h225 handler\r\n");
}

CMD_DEFINE_FN(debug_h245, "h245", 
	"H.245 Library Debugging", 1, 0)
{
	return vty_out(vty, "debug h245 handler\r\n");
}

CMD_DEFINE_FN(debug_GKTMP, "GKTMP",
	"GKTMP Library Debugging", 1, 0)
{
	return vty_out(vty, "debug GKTMP handler\r\n");
}

/*************************  H E L P  C O M M A N D S  ************************/

CMD_DEFINE_FN(cmd_help, "help",
	"Description of the interactive help system", 0, 0)
{
	return vty_out(vty, 
		"Help may be requested at any point in a command by entering\r\n"
		"a question mark '?'.  If nothing matches, the help list will\r\n"
		"be empty and you must backup until entering a '?' shows the\r\n"
		"available options.\r\n"
		"Two styles of help are provided:\r\n"
		"1. Full help is available when you are ready to enter a\r\n"
		"   command argument (e.g. 'show ?') and describes each possible\r\n"
		"   argument.\r\n"
		"2. Partial help is provided when an abbreviated argument is entered\r\n"
		"   and you want to know what arguments match the input\r\n"
		"   (e.g. 'show pr?'.)\r\n"
		"\r\n");
}

CMD_DEFINE_FN(help_editline, "editline", 
		"Editline functions.", 0, 1)
{
	return vty_out(vty, 
		"CTRL + A:    Move cursor to bol.\r\n"
		"CTRL + B:    Move cursor back.\r\n"
		"CTRL + E:    Move cursor to eol.\r\n"
		"CTRL + F:    Move cursor forward.\r\n"
		"CTRL + C:    Cancel actual Input.\r\n"
		"CTRL + R:    Reprint actual input line.\r\n"
		"CTRL + L:    Reprint actual input line.\r\n"
		"CTRL + N:    Move to next entry in history.\r\n"
		"CTRL + P:    Move to prev entry in history.\r\n"
		"CTRL + T:    Swap last characters tiped.\r\n"
		"CTRL + X:    Clear actual input line.\r\n"
		"CTRL + U:    Clear actual input line.\r\n"
		"CTRL + W:    Clear word.\r\n"
		
		"ESC:    Enter escape mode (See ESC characters).\r\n"
		"'<':    Move to first history entry.\r\n"
		"'>':    Move to last history entry.\r\n"
		"'A':    Move to next entry in history.\r\n"
		"'B':    Move to prev entry in history.\r\n"
		"'C':    Move cursor forward.\r\n"
		"'D':    Move cursor back.\r\n"
		"'b':    Move to begining of current.\r\n"
		"'Q':    Quoted insert mode.\r\n"
		"'q':    Quoted insert mode.\r\n"
		"'c':    Capitalize next word.\r\n"
		"'d':    Delete word.\r\n"
		"'f':    Go to the end of current word.\r\n"
		"'l':    Lowercase word.\r\n"
		"'u':    Uppercase word.\r\n"
		"DEL:    Clear actual input line.\r\n"	
		"\r\n");
}

CMD_DEFINE_FN(cmd_enable, "enable", "Turn on privileged commands", 0, 0)
{
	int failed = 0;
	
	cmd_set_vtymode(vty, ENABLE);
	
	while (ask_password(vty) < 0 && failed < 2)
		failed++;

	if (failed) {
		vty_out(vty, "\r\n%% Bad secrets\r\n");
		cmd_set_vtymode(vty, NORMAL);
	}
	return vty_out(vty, "\r\n");
}

CMD_DEFINE_FN(cmd_exit, "exit", "Exit from the EXEC", 0, 0)
{
	return cmd_set_vtymode(vty, NORMAL);
}

CMD_DEFINE_FN(exit_config, "exit", 
	"Exit from configure mode", 0, 0)
{
	return cmd_set_vtymode(vty, ENABLE);
}

CMD_DEFINE_FN(cmd_config, "configure", 
	"Enter configuration mode", 0, 0)
{
	return cmd_set_vtymode(vty, CONFIG);
}

CMD_DEFINE_FN(config_interface, "interface", 
	"Select an interface to configure", 0, 0)
{
	return cmd_set_vtymode(vty, INTERF);
}

CMD_DEFINE_FN(exit_config_interface, "exit", 
	"Exit from interface configuration mode", 0, 0)
{
	return cmd_set_vtymode(vty, CONFIG);
}

int commands_init(void)
{
	cmd_register(&exec_mode, &cmd_enable);
	cmd_register(&exec_mode, &cmd_config);
	cmd_register(&exec_mode, &cmd_exit);
	
	/* Show commands. */
	cmd_register(&exec_mode, &cmd_show);	
	
	cmd_register(&cmd_show.child_lst, &show_run);
	cmd_register(&show_run.child_lst, &show_run_interface);
	
	cmd_register(&cmd_show.child_lst, &show_ip);
	cmd_register(&cmd_show.child_lst, &show_interfaces);
	cmd_register(&cmd_show.child_lst, &show_debugging);
	cmd_register(&cmd_show.child_lst, &show_history);
	cmd_register(&cmd_show.child_lst, &show_clock);
	cmd_register(&cmd_show.child_lst, &show_users);
	
	cmd_register(&exec_mode, &cmd_clear);
	cmd_register(&cmd_clear.child_lst, &clr_counters);
	
	/* Debug commands. */
	cmd_register(&exec_mode, &cmd_debug);
	cmd_register(&cmd_debug.child_lst, &debug_h225);
	cmd_register(&cmd_debug.child_lst, &debug_h245);
	cmd_register(&cmd_debug.child_lst, &debug_GKTMP);
	
	/* Help commands. */
	cmd_register(&exec_mode, &cmd_help);
	cmd_register(&cmd_help.child_lst, &help_editline);
	

	/* Configuration Commands. */
	cmd_register(&config_mode, &exit_config);
	cmd_register(&config_mode, &config_interface);
	
	/* Configuration Interface Commands. */
	cmd_register(&interf_mode, &exit_config_interface);
	
	return 0;
}

init_call(commands_init);
