#include "lib.h"

static void sigchld_handler(int sig){
	pid_t pid ;
	wait(NULL);
	if(errno != ECHILD){
		fprintf(stderr,"workpid error: %s\n", strerror(errno));
		exit(0);
	}
}

int main(int argc, char **argv){
	int listenfd, connfd, port, clientlen;
	struct sockaddr_in clientaddr;
	signal(SIGCHLD,sigchld_handler);

	if (argc != 2) {
		exit(1);
	}
	port = atoi(argv[1]);

	listenfd = open_listenfd(port);

	while(1){
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (void *)&clientaddr, (socklen_t *)&clientlen);
		doit(connfd);
		close(connfd);

	}

	return 0;
}

