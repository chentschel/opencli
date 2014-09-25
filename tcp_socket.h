/**	
 *	tcp_socket.h - SHORT DESC..
 *
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

#ifndef _TCP_SOCKET_H_
#define _TCP_SOCKET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int sk_socket(int domain, int type, int protocol);
int sk_accept(int sfd, struct sockaddr_in *addr);
int sk_set_server(int sfd, const int port, int backlog);
int sk_set_nblock(int sfd);

ssize_t sk_send(int fd, const void *buff, size_t lenght, int flags);
ssize_t sk_recv(int fd, void *buff, size_t lenght, int flags);
size_t sk_send_all(int fd, const void *buff, size_t bcount);

#endif /* _TCP_SOCKET_H_ */
