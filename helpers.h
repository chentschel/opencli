/*****
 *	helpers.h - Funciones varias... 
 *
 *		2004, Christian Hentschel.
 *
 ***********************************************************************************/

#ifndef __HELPERS_H__
#define __HELPERS_H__


#include <stdio.h>
#include <pthread.h>	

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <time.h>

#include <signal.h>
#include <errno.h>


#define max(x,y) ((x) > (y) ? (x) : (y))
#define min(x,y) ((x) < (y) ? (x) : (y))


#define shutdown_fd(fdnum)\
	do {close(fdnum); fdnum = -1;} while(0)


#define M_ZERO(ptr)\
	do{ memset(ptr, 0,sizeof(typeof(*ptr))); }while(0)

/***************************************************************************
 * new_sock():
 * 	
 ***************************************************************************/
static __inline__ int new_sock(int domain, int type, int protocol)
{
	int sfd = socket(domain, type, protocol);

	if (sfd == -1){
		my_perror("socket()");
		return -1;
	}
	return sfd;
}


/***************************************************************************
 * set_client_fd(): 
 * 	
 ***************************************************************************/
static __inline__ int set_client_fd(int sfd, const char *ip_addr, const int port)
{
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip_addr); 		

	if( connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1 ){
		if (errno != EINPROGRESS){
			my_perror("connect()");
			return -1;
		}
	}
	return 0;
}


/***************************************************************************
 * set_server_fd(): 
 * 	
 ***************************************************************************/
static __inline__ int set_server_fd(int sfd, const int port, int backlog)
{
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons( port );
	addr.sin_addr.s_addr = INADDR_ANY;
		
	if( bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ){
		my_perror("bind()");
		return -1;
	}
	if( listen(sfd, backlog) == -1 ){
		my_perror("listen()");
		return -1;
	}
	return 0;
}


/***************************************************************************
 * my_recv(): recv() call, err handler.
 * 	
 ***************************************************************************/
static __inline__ ssize_t my_recv(int fd, void *buff, size_t lenght, int flags)
{
	ssize_t bcount;
	do
		bcount = recv(fd, buff, lenght, flags);
	while ((bcount < 0) && (errno == EINTR));

	if (bcount == -1)
		my_perror("recv()");

	return bcount;
}


/***************************************************************************
 * my_send(): send() call, err handler.
 * 	
 ***************************************************************************/
static __inline__ ssize_t my_send(int fd, void *buff, size_t lenght, int flags)
{
	ssize_t bcount;
	do 
		bcount = send(fd, buff, lenght, flags);
	while ((bcount < 0) && (errno == EINTR));

	if (bcount == -1)
		my_perror("send()");

	return bcount;
}


/***************************************************************************
 * set_nblock(): set a fd non bloking.
 * 	
 ***************************************************************************/
static __inline__ int set_nblock(int sfd)
{
	int flags = fcntl(sfd, F_GETFL, 0);
	
	if (flags < 0 || fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0){
		my_perror("fcntl()");
		return -1;
	}
	return 0;
}


/***********************************************************************************
 * do_fork():	Do a fork with err handle.
 *
 ***********************************************************************************/
static __inline__ void do_fork(void)
{
	pid_t	pid;
	
	if ((pid = fork()) == -1){
		perror("fork()");
		abort();
	}
	
	/* Parent ??
	 */
	if (pid != 0)
		exit(EXIT_SUCCESS);
}

#define MAXFD 	64

/***********************************************************************************
 * daemonize():	A function to daemonize the proc.
 *
 ***********************************************************************************/
static __inline__ void daemonize(void)
{
	do_fork();
		
	setsid();
	
	/* Wanna be child of init!! :)
	 */
	do_fork();

	if ( chdir("/") == -1){
		perror("chdir()");
		abort();
	}
	umask(0);
	
	/* Close all fds.. 
	 */
	int i; for(i=0; i<MAXFD; close(i++));
}


/***********************************************************************************
 * __malloc():	malloc w/err hdl.
 *
 ***********************************************************************************/
static __inline__ void *__malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL)
		abort();
	
	return ptr;
}


/***********************************************************************************
 * parseGUI():	Global Unique Identifier parser. Deletes "-" and " ".
 * 
 ***********************************************************************************/
static __inline__ void parseGUI(char *str, char *parsed)
{
	while( *str ){
		if( *str != '-' )
			*parsed++ = *str;
		str++;			
	}
	*parsed = '\0';
}


/***********************************************************************************
 * parsePort():	Port parser from IP or Transport address ([I:]IP:Port).
 *
 ***********************************************************************************/
static __inline__ char *parsePort(char *str)
{
	if (*str == 'I') 
		str +=2;
	while (*str && (*((++str)-1) != ':')); return str;
}


/***********************************************************************************
 * parseIPaddress():	IP parser from IP or Transport address ([I:]IP:Port).
 *
 ***********************************************************************************/
static __inline__ void parseIPaddress(char *str, char *parsed)
{
 	if (*str == 'I')
		str+=2; /* Elimino la I: .. */
						
	while (*str) {
		if (*str == ':')
			break;
		if (*str != '.')
			*parsed++ = *str;
		str++; 
	}
	*parsed = '\0';
}


/***********************************************************************************
 * parseAliasAddress():	string parser, looking for a token.
 *
 ***********************************************************************************/
static __inline__ void parseAliasAddress(char *str, char *token, char *parsed)
{
	if (str && (str = strstr(str, token))) {
		for (str+=2; *str && (*str != ' '); )
			*(parsed++) = *(str++);
	}
	*parsed = '\0';
}


#define HASH_TABLE_SIZE 12	/* It means 4096 places..*/
#define hashsize(n) ((unsigned int)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/****************************************************************************
 * hashStr():	Hash maker, given a string and strlen.
 *		
 ****************************************************************************/
static __inline__ unsigned int hashStr(char *key, unsigned int len)
{
	register unsigned int	hash, i;
  
	for (hash=0, i=0; i<len; ++i){
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
  
	return (hash & hashmask(HASH_TABLE_SIZE));  // 12 Bits de longitud para el Hash
} 


#endif /* __HELPERS_H__ */
