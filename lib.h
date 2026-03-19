#include <asm-generic/errno-base.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wait.h>

extern char **environ;

#define LISTENQ 1024
#define MAXLINE 1024
#define RIO_BUFSIZE 8192
#define MAXBUF 10000

typedef struct{
	int rio_fd;
	int rio_cnt;
	char *rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
} rio_t ;

int open_listenfd(int port);
int open_clientfd(char *hostname , int port);
ssize_t rio_read(rio_t *rp , char *usrbuf , size_t n);
ssize_t rio_readn(int fd , void *usrbuf, size_t n);
ssize_t rio_writen(int fd , void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp , int fd);
ssize_t rio_readlineb(rio_t *rp ,void *usrbuf , size_t maxlen);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
int parse_uri(char *uri, char *filename, char *cgiargs);
void doit(int fd);
void read_requesthdrs(rio_t *rp);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
