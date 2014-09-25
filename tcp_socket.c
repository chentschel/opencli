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
#include <errno.h>
#include <fcntl.h>

#include "tcp_socket.h"


/* Set a socket into listening mode. */
int sk_set_server(int sfd, const int port, int backlog)
{
	struct sockaddr_in addr = {
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(port),
		.sin_family = AF_INET
	};

	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
		perror("bind()");
		return -1;
	}
	if (listen(sfd, backlog) == -1){
		perror("listen()");
		return -1;
	}
	return 0;
}

int sk_socket(int domain, int type, int protocol)
{
	int sfd = socket(domain, type, protocol);

	if (sfd == -1){
		perror("socket()");
		return -1;
	}
	return sfd;
}

int sk_set_nblock(int sfd)
{
	int flags = fcntl(sfd, F_GETFL, 0);
	
	if (flags < 0 || fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0){
		perror("fcntl()");
		return -1;
	}
	return 0;
}

/* Accept, signal safe.*/
int sk_accept(int sfd, struct sockaddr_in *addr)
{
	int nfd, addrlen = sizeof(struct sockaddr_in);

	do {
		nfd = accept(sfd, (struct sockaddr *)addr, (socklen_t *)&addrlen);

	} while (nfd < 0 && errno == EINTR);
	
	if (nfd < 0)
		perror("accept()");

	return nfd;
}

/* Write to socket, signal safe. */
ssize_t sk_send(int fd, const void *buff, size_t lenght, int flags)
{
	ssize_t bcount;
	do 
		bcount = send(fd, buff, lenght, flags);
	while ((bcount < 0) && (errno == EINTR));

	if (bcount == -1)
		perror("send()");

	return bcount;
}

/* Send a stream to the network */
size_t sk_send_all(int fd, const void *buff, size_t bcount)
{
        register const char *ptr = (const char *)buff;
        register size_t bytes_left = bcount;

        while (bytes_left){
                register ssize_t rc;
                
		rc = sk_send(fd, ptr, bytes_left, 0);
		if (rc < 0)
			return -1;
		
                bytes_left -= rc;
                ptr += rc;
        }
        return bcount;
}

/* Read from socket, signal safe. */
ssize_t sk_recv(int fd, void *buff, size_t lenght, int flags)
{
	ssize_t bcount;
	do
		bcount = recv(fd, buff, lenght, flags);
	while ((bcount < 0) && (errno == EINTR));

	if (bcount == -1)
		perror("recv()");

	return bcount;
}
