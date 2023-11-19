#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct s_fd {
	char	*recv;
	char	*send;
	int		fd;
	int		id;
}	t_fd;

typedef struct s_serv {
	t_fd	fds[FD_SETSIZE];
	int		nfds;
	int		bfd;
	int		lid;
	int		sockfd;
	fd_set	reads;
	fd_set	writes;
	fd_set	errors;
	struct timeval	tm;
}	t_serv;

int	ft_error(t_serv *serv, char *msg) {
	write(2, msg, strlen(msg));
	if (!serv)
		return (1);
	for (int i = 0; i < serv->nfds; i++) {
		free(serv->fds[i].recv);
		serv->fds[i].recv = 0;
		free(serv->fds[i].send);
		serv->fds[i].send = 0;
	}
	return (1);
}

int	ft_strlen(char *str) {
	int	i;

	if (!str || !(*str))
		return (0);
	i = 0;
	while (str[i])
		i++;
	return (i);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int	ft_shrink(t_fd *fds, int nfds) {
	t_fd	tab[FD_SETSIZE];
	int		i;
	int		j;

	i = 0;
	j = 0;
	while (i < nfds) {
		if (fds[i].fd > 0)
			tab[j++] = fds[i];
		i++;
	}
	i = j;
	bzero(fds, FD_SETSIZE * sizeof(t_fd));
	while (--j >= 0)
		fds[j] = tab[j];
	return (i);
}

void	ft_prepare(t_serv *serv) {
	int	i;

	serv->nfds = ft_shrink(serv->fds, serv->nfds);
	FD_ZERO(&serv->reads);
	FD_ZERO(&serv->writes);
	FD_ZERO(&serv->errors);
	FD_SET(serv->sockfd, &serv->reads);
	i = 0;
	while (i < serv->nfds) {
		FD_SET(serv->fds[i].fd, &serv->reads);
		FD_SET(serv->fds[i].fd, &serv->writes);
		FD_SET(serv->fds[i].fd, &serv->errors);
		i++;
	}
	serv->tm.tv_sec = 1;
	serv->tm.tv_usec = 0;
}

int main(int ac, char **av) {
	struct sockaddr_in	servaddr, cli;
	int			sockfd, connfd;
	socklen_t	len;

	t_serv	serv;
	char	msg[1024];
	char	buf[1024];
	int		ret, n;

	if (ac != 2)
		return (ft_error(0, "Wrong number of arguments\n"));
	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		return (ft_error(0, "Fatal error\n"));
	bzero(&servaddr, sizeof(servaddr));

	printf("port: %d\n", atoi(av[1]));
	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));
 
	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		return (ft_error(0, "Fatal error\n"));
	if (listen(sockfd, 100) != 0)
		return (ft_error(0, "Fatal error\n"));


//	bzero(&serv, sizeof(t_serv));
	serv.sockfd = sockfd;
	serv.bfd = sockfd;
	serv.nfds = 0;
	serv.lid = 0;
	bzero(&serv.fds, sizeof(t_fd) * FD_SETSIZE);

	printf("sockfd: %d - bfd: %d\n", serv.sockfd, serv.bfd);

	while (42) {
		ft_prepare(&serv);

		if ((ret = select(serv.bfd + 1, &serv.reads, &serv.writes, &serv.errors, &serv.tm)) < 0)
			return (ft_error(&serv, "Fatal error\n"));
		printf("ret before: %d\n", ret);
		/*
		 * accept new connection
		*/
		if (FD_ISSET(serv.sockfd, &serv.reads)) {
			len = sizeof(cli);
			printf("accepting %d\n", serv.lid);
			connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
			if (connfd < 0)
				return (ft_error(&serv, "Fatal error\n"));
			serv.fds[serv.nfds].fd = connfd;
			serv.fds[serv.nfds].id = serv.lid++;
			if (!(serv.fds[serv.nfds].recv = (char *) calloc(1024, sizeof(char))))
				return (ft_error(&serv, "Fatal error\n"));
			if (connfd > serv.bfd)
				serv.bfd = connfd;
			sprintf(msg, "server: client %d just arrived\n", serv.lid - 1);
			for (int i = 0; i < serv.nfds; i++)
				if (!(serv.fds[i].send = str_join(serv.fds[i].send, msg)))
					return (ft_error(&serv, "Fatal error\n"));
			serv.nfds++;
			ret--;
			printf("client accepted %d\n", serv.fds[serv.nfds - 1].id);
		}

		for (int i = 0; i < serv.nfds; i++) {
			if (serv.fds[i].fd <= 0) {
				printf("skipping\n");
				continue ;
			}
			n = 0;
			/*
			 * read
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
				printf("receiving %d\n", serv.fds[i].id);
				do {
					bzero(buf, 1024);
					n = recv(serv.fds[i].fd, buf, 1023, MSG_DONTWAIT);
					serv.fds[i].recv = (char *) realloc(serv.fds[i].recv, ft_strlen(serv.fds[i].recv) + n + 1);
					if (!serv.fds[i].recv)
						return (ft_error(&serv, "Fatal error\n"));
					serv.fds[i].recv = strcat(serv.fds[i].recv, buf);
				} while (n == 1023);
//				printf("|\n%s|\n", serv.fds[i].recv);
//				sleep(15);
				/*
				 * close connection
				*/
				if (!n) {
					printf("closing connection %d\n", serv.fds[i].id);
					serv.fds[i].fd = -1;
					free(serv.fds[i].recv);
					serv.fds[i].recv = 0;
					free(serv.fds[i].send);
					serv.fds[i].send = 0;
					bzero(msg, 1024);
					sprintf(msg, "server: client %d just left\n", serv.fds[i].id);
					for (int j = 0; j < serv.nfds; j++) {
						if (j == i || serv.fds[j].fd <= 0)
							continue ;
						printf("set send for %d\n", serv.fds[j].id);
						if (!(serv.fds[j].send = str_join(serv.fds[j].send, msg)))
							return (ft_error(&serv, "Fatal error\n"));
					}
					printf("sent message to everyone\n");
				}
				else {
//					bzero(msg, 1024);
//					sprintf(msg, "client %d: %s", serv.fds[i].id, serv.fds[i].recv);
//					for (int j = 0; j < serv.nfds; j++) {
//						if (j == i || serv.fds[j].fd <= 0)
//							continue ;
//						printf("set send for %d\n", serv.fds[j].id);
//						if (!(serv.fds[j].send = str_join(serv.fds[j].send, msg)))
//							return (ft_error(&serv, "Fatal error\n"));
//					}
//					printf("sent message to everyone\n");
					char	*str = serv.fds[i].recv;
					for (int j = 0, k = 0; ; j++) {
						if (!str[j]) {
							bzero(msg, 1024);
							sprintf(msg, "client %d: %s", serv.fds[i].id, str + k);
							for (int j = 0; j < serv.nfds; j++) {
								if (j == i || serv.fds[j].fd <= 0)
									continue ;
								printf("set send for %d\n", serv.fds[j].id);
								if (!(serv.fds[j].send = str_join(serv.fds[j].send, msg)))
									return (ft_error(&serv, "Fatal error\n"));
							}
							break ;
						}
						else if (str[j] == 10) {
							str[j] = 0;
							bzero(msg, 1024);
							sprintf(msg, "client %d: %s\n", serv.fds[i].id, str + k);
							for (int j = 0; j < serv.nfds; j++) {
								if (j == i || serv.fds[j].fd <= 0)
									continue ;
								printf("set send for %d\n", serv.fds[j].id);
								if (!(serv.fds[j].send = str_join(serv.fds[j].send, msg)))
									return (ft_error(&serv, "Fatal error\n"));
							}
							k = ++j;
							if (!str[j])
								break ;
						}
					}
					free(serv.fds[i].recv);
					serv.fds[i].recv = 0;
				}
				ret--;
			}
			/*
			 * write
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.writes)) {
				printf("sending %d\n", serv.fds[i].id);
				if (serv.fds[i].send) {
					n = send(serv.fds[i].fd, serv.fds[i].send, strlen(serv.fds[i].send), MSG_DONTWAIT);
					free(serv.fds[i].send);
					serv.fds[i].send = 0;
				}
				else
					printf("nothing to send to %d\n", serv.fds[i].id);
				if (n < 0) {
					printf("closing connection %d\n", serv.fds[i].id);
					serv.fds[i].fd = -1;
					free(serv.fds[i].recv);
					serv.fds[i].recv = 0;
					free(serv.fds[i].send);
					serv.fds[i].send = 0;
					bzero(msg, 1024);
					sprintf(msg, "server: client %d just left\n", serv.fds[i].id);
					for (int j = 0; j < serv.nfds; j++) {
						if (j == i || serv.fds[j].fd <= 0)
							continue ;
						if (!(serv.fds[j].send = str_join(serv.fds[j].send, msg)))
							return (ft_error(&serv, "Fatal error\n"));
					}
					printf("sent message to everyone\n");
				}
				else {
					printf("%d sent to %d\n", n, serv.fds[i].id);
				}
				ret--;
			}
			/*
			 * error
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.errors)) {
				printf("got an error on %d\n", serv.fds[i].id);
				serv.fds[i].fd = -1;
				free(serv.fds[i].recv);
				serv.fds[i].recv = 0;
				free(serv.fds[i].send);
				serv.fds[i].send = 0;
				ret--;
			}
			printf("ret after: %d\n", ret);
			fflush(stdout);
		}
	}
	return (0);
}
