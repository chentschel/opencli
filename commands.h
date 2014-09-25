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


#ifndef __PARSER_H__
#define __PARSER_H__

enum ACT_ON_FINDER 	{ FIND, COMPLETE, C_HELP, HELP };
enum CMD_STATE		{ CMD_FOUND, CMD_INVALID, CMD_AMBIGUOUS, CMD_INCOMPLETE };

/**	CMD_INCOMPLETE = cmd is expected to be followed by params.
 *	CMD_INVALID = our parser did not find a match for the cmd string.
 *	CMD_AMBIGUOUS = the parser found more than a match for the string.
 */
 
typedef struct CLI_CMD_ {
	
	struct list_head child_lst, parent_lst;

	char *cmda, *summary;
	int (*handler)(CL_SOCK *);
	int privilege;
	int final;

} CLI_CMD;


int cmd_finder(char **argv, struct list_head *cmd_lst, 
		CLI_CMD **m_match, int act, int privilege);
		
extern __inline__ char *cmd_is_completion(char *cmd, char *real_cmd);
extern __inline__ struct list_head *cmd_get_cmdroot(CL_SOCK *vty);
extern __inline__ const char *cmd_mode_prompt(CL_SOCK *vty);

const char *cmd_get_hostname(void);

void cmd_register(struct list_head *listPtr, CLI_CMD *cmd);


/** Use these macros to implement a new command.!
 */
#define CMD_DEFINE(cmd_name, cmd_str, cmd_summ, fin_fl, pri_lv, fn_hdl)\
	CLI_CMD cmd_name = { \
		.cmda = cmd_str, \
		.summary = cmd_summ, \
		.final = fin_fl, \
		.privilege = pri_lv, \
		.handler = fn_hdl \
	}\

/** A new command with a handler function will use this. 
 */ 
#define CMD_DEFINE_FN(cmd_name, cmd_str, cmd_summ, fin_fl, pri_lv)\
	static int cmd_name ## _hdl(CL_SOCK *);\
	CMD_DEFINE(cmd_name, cmd_str, cmd_summ, fin_fl, pri_lv, cmd_name ## _hdl);\
	static int cmd_name ## _hdl(CL_SOCK *vty)\
\

#endif /*__PARSER_H__*/
