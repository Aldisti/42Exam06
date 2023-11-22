#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct s_fd {
	int		fd;
	int		id;
}	t_fd;

typedef struct s_serv {
	char	msg[200015];
	char	buf[200000];
	t_fd	fds[128];
	int		nfds;
	int		sockfd;
	int		bfd;
	int		lid;
	fd_set	reads;
	struct timeval	tm;
}	t_serv;

int	ft_strlen(char *str) {
	int	i = 0;

	while (str && str[i])
		i++;
	return (i);
}

int	ft_error(t_serv *serv, char *str) {
	(void) serv;
	if (str)
		write(2, str, ft_strlen(str));
	if (!serv)
		return (1);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd < 0)
			continue ;
		close(serv->fds[i].fd);
	}
	FD_ZERO(&serv->reads);
	bzero(serv->msg, sizeof(serv->msg));
	bzero(serv->buf, sizeof(serv->buf));
	return (1);
}

void	ft_prepare(t_serv *serv) {
	if (!serv)
		return ;
	FD_ZERO(&serv->reads);
	FD_SET(serv->sockfd, &serv->reads);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd < 0)
			continue ;
		FD_SET(serv->fds[i].fd, &serv->reads);
	}
	serv->tm.tv_sec = 1;
	serv->tm.tv_usec = 0;
}

int	ft_accept(t_serv *serv) {
	struct sockaddr_in	cli;
	socklen_t			len;
	int					cfd, n;

	len = sizeof(cli);
	cfd = accept(serv->sockfd, (struct sockaddr *)&cli, &len);
	if (cfd < 0)
		return (0);
	serv->fds[serv->nfds].fd = cfd;
	serv->fds[serv->nfds].id = serv->lid;
	serv->bfd = (cfd > serv->bfd) ? cfd : serv->bfd;
	FD_SET(cfd, &serv->reads);
	bzero(serv->msg, sizeof(serv->msg));
	sprintf(serv->msg, "server: client %d just arrived\n", serv->lid);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd < 0)
			continue ;
		n = send(serv->fds[i].fd, serv->msg, ft_strlen(serv->msg), MSG_DONTWAIT);
		if (n < 0)
			return (0);
	}
	serv->nfds++;
	serv->lid++;
	return (1);
}

int	ft_close(t_serv *serv, int index) {
	int	n;

	bzero(serv->msg, sizeof(serv->msg));
	sprintf(serv->msg, "server: client %d just left\n", serv->fds[index].id);
	for (int i = 0; i < serv->nfds; i++) {
		if (i == index || serv->fds[i].fd < 0)
			continue ;
		n = send(serv->fds[i].fd, serv->msg, ft_strlen(serv->msg), MSG_DONTWAIT);
		if (n < 0)
			return (0);
	}
	close(serv->fds[index].fd);
	FD_CLR(serv->fds[index].fd, &serv->reads);
	serv->fds[index].fd = -1;
	return (1);
}

int	ft_read(t_serv *serv, int index) {
	int	n;

	bzero(serv->msg, sizeof(serv->msg));
	sprintf(serv->msg, "client %d: %s", serv->fds[index].id, serv->buf);
	for (int i = 0; i < serv->nfds; i++) {
		if (i == index || serv->fds[i].fd < 0)
			continue ;
		n = send(serv->fds[i].fd, serv->msg, ft_strlen(serv->msg), MSG_DONTWAIT);
		if (n < 0)
			return (0);
	}
	return (1);
}

int main(int ac, char **av) {
	struct sockaddr_in	servaddr;
	t_serv				serv;
	int					ret, n;

	if (ac != 2)
		return (ft_error(0, "Wrong number of arguments\n"));
	bzero(&serv, sizeof(t_serv));
	// socket create and verification
	serv.sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serv.sockfd == -1)
		return (ft_error(0, "Fatal error\n"));
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));
 
	// Binding newly created socket to given IP and verification
	if ((bind(serv.sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		return (ft_error(0, "Fatal error\n"));
	if (listen(serv.sockfd, 128) != 0)
		return (ft_error(0, "Fatal error\n"));

	serv.nfds = 0;
	serv.bfd = serv.sockfd;
	serv.lid = 0;

	while (42) {
		ft_prepare(&serv);
		ret = select(serv.bfd + 1, &serv.reads, 0, 0, &serv.tm);
//		printf("select %d\n", ret);
		if (ret < 0)
			return (ft_error(&serv, "Fatal error\n"));
		if (FD_ISSET(serv.sockfd, &serv.reads)) {
//			printf("new connection %d\n", serv.lid);
			if (!ft_accept(&serv))
				return (ft_error(&serv, "Fatal error\n"));
			continue ;
		}
		for (int i = 0; i < serv.nfds; i++) {
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
				bzero(serv.buf, sizeof(serv.buf));
				n = recv(serv.fds[i].fd, serv.buf, sizeof(serv.buf), MSG_DONTWAIT);
				if (n == 0) {
//					printf("close connection %d\n", serv.fds[i].id);
					if (!ft_close(&serv, i))
						return (ft_error(&serv, "Fatal error\n"));
				}
				else {
//					printf("receiving %d\n", serv.fds[i].id);
					if (!ft_read(&serv, i))
						return (ft_error(&serv, "Fatal error\n"));
				}
			}
		}
	}
	return (0);
}

