#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

void error(char *msg) {
	perror(msg);
	exit(1);
}

int main(int argc, char **argv) {
	int parentfd; /* parent socket */
	int childfd; /* child socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	char buf[BUFSIZE]; /* message buffer */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	int connectcnt; /* number of connection requests */
	int notdone;
	fd_set readfds;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);

	parentfd = socket(AF_INET, SOCK_STREAM, 0);
	if (parentfd < 0) 
		error("ERROR opening socket");

	optval = 1;
	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
			 (const void *)&optval , sizeof(int));

	bzero((char *) &serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

	serveraddr.sin_port = htons((unsigned short)portno);

	if (bind(parentfd, (struct sockaddr *) &serveraddr, 
		 sizeof(serveraddr)) < 0) 
		error("ERROR on binding");

	if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
		error("ERROR on listen");


	clientlen = sizeof(clientaddr);
	notdone = 1;
	connectcnt = 0;
	printf("server> ");
	fflush(stdout);

	while (notdone) {

		/* 
		 * select: Has the user typed something to stdin or 
		 * has a connection request arrived?
		 */
		FD_ZERO(&readfds); /* initialize the fd set */
		FD_SET(parentfd, &readfds); /* add socket fd */
		FD_SET(0, &readfds); /* add stdin fd (0) */
		if (select(parentfd+1, &readfds, 0, 0, 0) < 0) {
			error("ERROR in select");
		}

		/* if the user has entered a command, process it */
		if (FD_ISSET(0, &readfds)) {
			fgets(buf, BUFSIZE, stdin);
			switch (buf[0]) {
				case 'c': /* print the connection cnt */
					printf("Received %d connection requests so far.\n", connectcnt);
					printf("server> ");
					fflush(stdout);
					break;
				case 'q': /* terminate the server */
					notdone = 0;
					break;
				default: /* bad input */
					printf("ERROR: unknown command\n");
					printf("server> ");
					fflush(stdout);
			}
		}		

		/* if a connection request has arrived, process it */
		if (FD_ISSET(parentfd, &readfds)) {
			/* 
			 * accept: wait for a connection request 
			 */
			childfd = accept(parentfd, 
					 (struct sockaddr *) &clientaddr, &clientlen);
			if (childfd < 0) 
				error("ERROR on accept");
			connectcnt++;
			
			/* 
			 * read: read input string from the client
			 */
			bzero(buf, BUFSIZE);
			n = read(childfd, buf, BUFSIZE);
			if (n < 0) 
				error("ERROR reading from socket");
			
			/* 
			 * write: echo the input string back to the client 
			 */
			n = write(childfd, buf, strlen(buf));
			if (n < 0)
				error("ERROR writing to socket");

			close(childfd);
		}
	}

	/* clean up */
	printf("Terminating server.\n");
	close(parentfd);
	exit(0);
}

