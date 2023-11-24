#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct s_fd {
	int	fd;
	int	id;
}	t_fd;

typedef struct s_serv {
	char	msg[500150];
	char	buf[500000];
	t_fd	fds[4096];
	int		nfds;
	int		bfd;
	int		lid;
	int		sockfd;
	fd_set	reads;
	struct timeval	tm;
}	t_serv;

int	ft_error(t_serv *serv, char *str) {
	if (str)
		write(2, str, strlen(str));
	if (!serv)
		return (1);
	FD_ZERO(&serv->reads);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd <= 0)
			continue ;
		close(serv->fds[i].fd);
		serv->fds[i].fd = -1;
	}
	return (1);
}

void	ft_prepare(t_serv *serv) {
	FD_ZERO(&serv->reads);
	FD_SET(serv->sockfd, &serv->reads);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd < 0)
			continue ;
		FD_SET(serv->fds[i].fd, &serv->reads);
	}
}

int	ft_send_all(t_serv *serv, int index) {
	int	len, n;

	len = strlen(serv->msg);
	for (int i = 0; i < serv->nfds; i++) {
		if (i == index || serv->fds[i].fd <= 0)
			continue ;
		n = send(serv->fds[i].fd, serv->msg, len, MSG_DONTWAIT);
		if (n < 0)
			return (0);
	}
	return (1);
}

int	ft_accept(t_serv *serv) {
	struct sockaddr_in	cli;
	socklen_t			len;
	int					cfd;

	cfd = accept(serv->sockfd, (struct sockaddr *)&cli, &len);
	if (cfd == 0)
		return (0);
	serv->fds[serv->nfds].fd = cfd;
	serv->fds[serv->nfds].id = serv->lid;
	serv->bfd = (cfd > serv->bfd) ? cfd : serv->bfd;
	bzero(serv->msg, sizeof(serv->msg));
	sprintf(serv->msg, "server: client %d just arrived\n", serv->lid);
	if (!ft_send_all(serv, -1))
		return (0);
	serv->nfds++;
	serv->lid++;
	return (1);
}

int	ft_close(t_serv *serv, int index) {
	bzero(serv->msg, sizeof(serv->msg));
	sprintf(serv->msg, "server: client %d just left\n", serv->fds[index].id);
	if (!ft_send_all(serv, index))
		return (0);
	close(serv->fds[index].fd);
	serv->fds[index].fd = -1;
	return (1);
}

int	ft_read(t_serv *serv, int index) {
	int	len, k;

	len = strlen(serv->buf);
	if (serv->buf[len - 1] != 10)
		return (ft_close(serv, index));
	k = 0;
	for (int i = 0, j = 0; i < len; i++) {
		if (serv->buf[i] != 10 && serv->buf[i])
			continue ;
		k = (serv->buf[i] == 0);
		serv->buf[i] = 0;
		bzero(serv->msg, sizeof(serv->msg));
		sprintf(serv->msg, "client %d: %s\n", serv->fds[index].id, serv->buf + j);
		if (!ft_send_all(serv, index))
			return  (0);
		j = i + 1;
		if (k)
			break ;
	}
	return (1);
}

int	main(int ac, char **av) {
	struct sockaddr_in	servaddr;
	t_serv				serv;
	int					ret, n, j;

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
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	// Binding newly created socket to given IP and verification
	if ((bind(serv.sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		return (ft_error(0, "Fatal error\n"));
	if (listen(serv.sockfd, FD_SETSIZE) != 0)
		return (ft_error(0, "Fatal error\n"));

	serv.nfds = 0;
	serv.bfd = serv.sockfd;
	serv.lid = 0;
	serv.tm.tv_sec = 0;
	serv.tm.tv_usec = 0;

	while (42) {
		ft_prepare(&serv);
		ret = select(serv.bfd + 1, &serv.reads, 0, 0, &serv.tm);
		if (ret < 0)
			return (ft_error(&serv, "Fatal error\n"));
		if (FD_ISSET(serv.sockfd, &serv.reads)) {
			if (!ft_accept(&serv))
				return (ft_error(&serv, "Fatal error\n"));
			continue ;
		}
		for (int i = 0; i < serv.nfds; i++) {
			if (serv.fds[i].fd <= 0)
				continue ;
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
				j = 0;
				bzero(serv.buf, sizeof(serv.buf));
				do {
					n = recv(serv.fds[i].fd, serv.buf + j, 2048, 0);
					j += n;
				} while (n == 2048);
				if (serv.buf[0] == 0) {
					if (!ft_close(&serv, i))
						return (ft_error(&serv, "Fatal error\n"));
				}
				else if (j < 0)
					return (ft_error(&serv, "Fatal error\n"));
				else {
					if (!ft_read(&serv, i))
						return (ft_error(&serv, "Fatal error\n"));
				}
			}
		}
	}
	return (0);
}

