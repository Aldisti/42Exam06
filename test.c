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
	t_fd			fds[FD_SETSIZE];
	int				nfds;
	int				bfd;
	int				lid;
	int				sockfd;
	fd_set			reads;
	fd_set			writes;
	fd_set			errors;
	struct timeval	tm;
}	t_serv;

void	ft_free(void **ptr) {
	free(*ptr);
	*ptr = 0;
}

int	ft_strlen(char *str) {
	int	i = 0;

	if (!str)
		return (0);
	while (str[i])
		i++;
	return (i);
}

int	ft_error(t_serv *serv, char *str) {
	write(2, str, ft_strlen(str));
	if (!serv)
		return (1);
	for (int i = 0; i < serv->nfds; i++) {
		ft_free((void **) &serv->fds[i].recv);
		ft_free((void **) &serv->fds[i].send);
	}
	return (1);
}

void	ft_prepare(t_serv *serv) {
	FD_ZERO(&serv->reads);
	FD_ZERO(&serv->writes);
	FD_ZERO(&serv->errors);
	FD_SET(serv->sockfd, &serv->reads);
	for (int i = 0; i < serv->nfds; i++) {
		FD_SET(serv->fds[i].fd, &serv->reads);
		FD_SET(serv->fds[i].fd, &serv->writes);
		FD_SET(serv->fds[i].fd, &serv->errors);
	}
	serv->tm.tv_sec = 1;
	serv->tm.tv_usec = 0;
}

char	*ft_strjoin(char *s1, char *s2, int to_free) {
	char	*str;
	int		i;
	int		j;

	str = (char *) calloc(ft_strlen(s1) + ft_strlen(s2) + 1, sizeof(char));
	if (!str) {
		if (to_free == 1)
			ft_free((void **) &s1);
		else if (to_free == -1)
			ft_free((void **) &s2);
		return (0);
	}
	i = 0;
	while (s1 && s1[i]) {
		str[i] = s1[i];
		i++;
	}
	j = 0;
	while (s2 && s2[j])
		str[i++] = s2[j++];
	if (to_free == 1)
		ft_free((void **) &s1);
	else if (to_free == -1)
		ft_free((void **) &s2);
	return (str);
}

int	ft_send_all(t_serv *serv, char *str, int index) {
	char	msg[1024];

	bzero(msg, 1024);
	sprintf(msg, "client %d: %s", serv->fds[index].id, str);
	for (int i = 0; i < serv->nfds; i++) {
		if (i == index || serv->fds[i].fd < 0)
			continue ;
		serv->fds[i].send = ft_strjoin(serv->fds[i].send, msg, 1);
		if (!serv->fds[i].send)
			return (0);
	}
	return (1);
}

int	ft_accept(t_serv *serv) {
	struct sockaddr_in	cli;
	socklen_t			len;
	char				msg[256];
	int					connfd;

	len = sizeof(cli);
	connfd = accept(serv->sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0)
		return (0);
	serv->fds[serv->nfds].fd = connfd;
	serv->fds[serv->nfds].id = serv->lid++;
	serv->fds[serv->nfds].recv = 0;
	serv->fds[serv->nfds].send = 0;
	if (connfd > serv->bfd)
		serv->bfd = connfd;
	bzero(msg, 256);
	sprintf(msg, "server: client %d just arrived\n", serv->lid - 1);
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd < 0 || serv->fds[i].fd == connfd)
			continue ;
		serv->fds[i].send = ft_strjoin(serv->fds[i].send, msg, 1);
		if (!serv->fds[i].send)
			return (0);
	}
	serv->nfds++;
	printf("connection accepted %d\n", serv->lid - 1);
	return (1);
}

int	ft_get_msg(char **buf, char **msg) {
	char	*new_buf;
	int		i, j;

	if (!(*buf))
		return (0);
	i = 0;
	while ((*buf)[i] && (*buf)[i] != 10)
		i++;
	*msg = (char *) calloc(i + 2, sizeof(char));
	if (!(*msg))
		return (0);
	i = 0;
	while ((*buf)[i] && (*buf)[i] != 10) {
		(*msg)[i] = (*buf)[i];
		i++;
	}
	(*msg)[i] = (*buf)[i];
	if ((*buf)[i] == 10)
		i++;
//	printf("msg: |%s|\n", *msg);
	if (!(*buf)[i] || ft_strlen(*buf) == i) {
		ft_free((void **) buf);
		return (1);
	}
	new_buf = (char *) calloc(ft_strlen(*buf) - i + 1, sizeof(char));
	if (!new_buf) {
		ft_free((void **) msg);
		return (0);
	}
	j = 0;
	while ((*buf)[i])
		new_buf[j++] = (*buf)[i++];
	ft_free((void **) buf);
	*buf = new_buf;
//	printf("buf: |%s|\n", *buf);
	return (1);
}

int	ft_read(t_serv *serv, int i) {
	char	buff[256];
	char	*tmp = 0;
	int		n = 0;

	do {
		bzero(buff, 256);
		n = recv(serv->fds[i].fd, buff, 256, MSG_DONTWAIT);
		serv->fds[i].recv = ft_strjoin(serv->fds[i].recv, buff, 1);
		if (!serv->fds[i].recv)
			return (0);
	} while (n == 255);
	if (n == 0 || n < 0) {
		char	msg[1024];

		printf("disconnecting %d\n", serv->fds[i].id);
		bzero(msg, 1024);
		sprintf(msg, "server: client %d just left\n", serv->fds[i].id);
		for (int j = 0; j < serv->nfds; j++) {
			if (j == i || serv->fds[j].fd < 0)
				continue ;
		serv->fds[j].send = ft_strjoin(serv->fds[j].send, msg, 1);
		if (!serv->fds[j].send)
			return (0);
		}
		ft_free((void **) &serv->fds[i].recv);
		ft_free((void **) &serv->fds[i].send);
		close(serv->fds[i].fd);
		serv->fds[i].fd = -1;
	}
	else {
		while (serv->fds[i].recv && *(serv->fds[i].recv)) {
			n = ft_get_msg(&serv->fds[i].recv, &tmp);
			if (!n && serv->fds[i].recv)
				return (0);
			if (!tmp || !(*tmp))
				return (1);
			n = ft_send_all(serv, tmp, i);
			ft_free((void **) &tmp);
			if (!n)
				return (0);
		}
	}
	ft_free((void **) &serv->fds[i].recv);
	return (1);
}

int	ft_write(t_serv *serv, int i) {
	int	n;

	if (!serv->fds[i].send)
		return (1);
	n = send(serv->fds[i].fd, serv->fds[i].send, ft_strlen(serv->fds[i].send), MSG_DONTWAIT);
	if (!n || n < 0) {
		printf("cannot send to %d\n", serv->fds[i].id);
		close(serv->fds[i].fd);
		ft_free((void **) &serv->fds[i].recv);
		serv->fds[i].fd = -1;
	}
	else {
		printf("%dB sent to %d\n", n, serv->fds[i].id);
	}
	ft_free((void **) &serv->fds[i].send);
	return (1);
}

int main(int ac, char **av) {
	int			sockfd;
	struct sockaddr_in	servaddr;

	t_serv	serv;
	int		ret;

	if (ac != 2)
		return (ft_error(0, "Wrong number of arguments\n"));

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		return (ft_error(0, "Fatal error\n"));
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));
 
	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		return (ft_error(0, "Fatal error\n"));
	if (listen(sockfd, 10) != 0)
		return (ft_error(0, "Fatal error\n"));

	bzero(&serv, sizeof(t_serv));
	serv.sockfd = sockfd;
	serv.nfds = 0;
	serv.bfd = sockfd;
	serv.lid = 0;


	while (42) {
		ft_prepare(&serv);
		if ((ret = select(serv.bfd + 1, &serv.reads, &serv.writes, &serv.errors, &serv.tm)) < 0)
			return (ft_error(&serv, "Fatal error\n"));
		(void) ret;

		if (FD_ISSET(serv.sockfd, &serv.reads)) {
			if (!ft_accept(&serv))
				return (ft_error(&serv, "Fatal error\n"));
		}
		for (int i = 0; i < serv.nfds; i++) {
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
				if (!ft_read(&serv, i))
					return (ft_error(&serv, "Fatal error\n"));
			}
			if (FD_ISSET(serv.fds[i].fd, &serv.writes)) {
				if (!ft_write(&serv, i))
					return (ft_error(&serv, "Fatal error\n"));
			}
			if (FD_ISSET(serv.fds[i].fd, &serv.errors)) {
				printf("ERROR %d\n", serv.fds[i].id);
			}
		}
	}
	return (0);
}
