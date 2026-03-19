#include "lib.h"

ssize_t rio_readn(int fd, void *usrbuf, size_t n){
	size_t n_left = n ;
	ssize_t nread;
	char *bufp = usrbuf;

	while(n_left > 0){
		if((nread = read(fd, bufp, n_left)) < 0){
			if(errno == EINTR)
				nread = 0 ;
			else
				return -1 ;
		}
		else if (nread == 0) {
			break ;
		}
		n_left -= nread ;
		bufp += nread ;
	}
	return n - n_left ;
};



ssize_t rio_writen(int fd, void *usrbuf, size_t n){
	size_t n_left = n ;
	ssize_t nwrite;
	char *bufp = usrbuf;

	while(n_left > 0){
		if((nwrite = write(fd, bufp, n_left)) < 0){
			if(errno == EINTR)
				nwrite = 0 ;
			else
				return -1 ;
		}
		else if (nwrite == 0) {
			break ;
		}
		n_left -= nwrite ;
		bufp += nwrite ;
	}

	return n ;
};


void rio_readinitb(rio_t *rp , int fd){
	rp->rio_fd = fd;
	rp->rio_cnt = 0;
	rp->rio_bufptr = rp->rio_buf;
}


ssize_t rio_read(rio_t *rp , char *usrbuf , size_t n){
	int cnt = n ;

	while(rp->rio_cnt <= 0){
		rp->rio_cnt = read(rp->rio_fd , rp->rio_buf , sizeof(rp->rio_buf));

		if(rp->rio_cnt < 0){
			if (errno != EINTR)
				return -1 ;
		}
		else if (rp->rio_cnt == 0)
			return 0 ;
		else
			rp->rio_bufptr = rp->rio_buf ;
	}
	
	if(rp->rio_cnt < n)
		cnt = rp->rio_cnt ;
	memcpy(usrbuf, rp->rio_bufptr, cnt);
	rp->rio_bufptr += cnt ;
	rp->rio_cnt -= cnt ;
	return cnt ;

}

ssize_t rio_readlineb(rio_t *rp , void *usrbuf , size_t maxlen){

	int n,rc ;
	char c, *bufp = usrbuf ;

	for(n = 1 ; n < maxlen ; ++n){
		if((rc=rio_read(rp, &c, 1)) == 1){
			*bufp++ = c ;
			if(c == '\n')
				break ;
		}
		else if(rc == 0){
			if(n == 1)
				return 0;
			else 
				break ;
		}
		else 
			return -1 ;
	}
	*bufp = 0 ;
	return n ;

};

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n){
	size_t nleft = n ;
	ssize_t nread ;
	char *bufp = usrbuf ;

	while(nleft > 0){
		if( (nread = rio_read(rp, bufp, nleft)) < 0 ){
			if(errno == EINTR)
				nread = 0 ;
			else
				return -1 ;
		}
		else if(nread == 0)
			break ;
		nleft -= nread ;
		bufp += nread ;
	}

	return n - nleft ;

};

int open_clientfd(char *hostname, int port){

	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	if((hp = gethostbyname(hostname)) == NULL)
		return -2;
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	if(connect(clientfd, (void *)&serveraddr, sizeof(serveraddr)) < 0)
		return -1 ;

	return clientfd ;

}

int open_listenfd(int port){
	
	int listenfd , optval = 1 ;
	struct sockaddr_in serveraddr;

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
		return -1 ;

	if(setsockopt(listenfd, SOL_SOCKET ,SO_REUSEADDR , (const void *)&optval, sizeof(int)) < 0 )
		return -1 ;

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET ;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);

	if(bind(listenfd ,(void *)&serveraddr , sizeof(serveraddr)) < 0 )
		return -1 ;

	if(listen(listenfd, LISTENQ) < 0)
		return -1 ;

	return listenfd ;
}

void doit(int fd){
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio ;

	rio_readinitb(&rio, fd);
	rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method , uri , version);
	if(strcasecmp(method, "GET")){
		clienterror(fd, method, "501", "Not Implemented", "does not implement this method");
		return ;
	}
	printf("%s\n",buf);
	read_requesthdrs(&rio);

	is_static = parse_uri(uri, filename, cgiargs);
	
	if(stat(filename,&sbuf) < 0) {
		clienterror(fd, filename, "404", "Not found", "The server couldn't find this file");
		return ;
	}

	if(is_static){
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "The server couldn't read this file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}
	else{
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "The server couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
	char buf[MAXLINE], body[MAXBUF];

	sprintf(body, "<html><title>Error</title>");
	sprintf(body, "%s<body bgcolor = \"ffffff\">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, longmsg, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum , shortmsg);
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int) strlen(body));
	rio_writen(fd, buf, strlen(buf));
	rio_writen(fd, body, strlen(body));

}

void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	rio_readlineb(rp, buf, MAXLINE);
	while(strcmp(buf, "\r\n")) {
		rio_readlineb(rp, buf, MAXLINE);
		printf("%s",buf);
	}
}

int parse_uri(char *uri, char *filename, char *cgiargs){
	char *ptr;

	if(!strstr(uri , "cgi-bin")) {
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if(uri[strlen(uri)-1] == '/')
			strcat(filename,"home.html");
		return 1;
	}
	else {
		ptr = strchr(uri, '?');
		if(ptr) {
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		}
		else
			strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;

	}

}

void serve_static(int fd, char *filename, int filesize){
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK \r\n");
	sprintf(buf, "%sServer: Web Server\r\n",buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	rio_writen(fd, buf, strlen(buf));

	srcfd = open(filename, O_RDONLY, 0);
	srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	close(srcfd);
	rio_writen(fd, srcp, filesize);
	munmap(srcp, filesize);

}

void get_filetype(char *filename, char *filetype){
	if(strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if(strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if(strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpg");
	else if(strstr(filename, ".mpg"))
		strcpy(filetype, "video/mpg");
	else
		strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs){
	char buf[MAXLINE], *emptylist[] = {NULL};

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Web Server\r\n");
	rio_writen(fd, buf, strlen(buf));

	if(fork() == 0) {
		setenv("QUERY_STRING", cgiargs, 1);
		dup2(fd, STDOUT_FILENO);
		execve(filename, emptylist, environ);
	}

}
