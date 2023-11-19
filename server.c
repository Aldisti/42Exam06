/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   my_server.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: adi-stef <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/11/16 13:35:44 by adi-stef          #+#    #+#             */
/*   Updated: 2023/11/16 15:37:03 by adi-stef         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFSIZE 1024
#define SOCKADDR struct sockaddr_in

typedef struct s_fd {
	char	*send;
	char	*recv;
	int		fd;
	int		id;
}	t_fd;

typedef struct s_serv {
	t_fd			fds[FD_SETSIZE]; // array to store all the fds
	int				nfds; // number of fds
	int				bfd; // biggest fd
	int				lid; // last id
	fd_set			reads;
	fd_set			writes;
	fd_set			errors;
	int				port;
	SOCKADDR		server_addr;
	SOCKADDR		client_addr;
	struct timeval	tm;
}	t_serv;

void	ft_free(void **ptr) {
	free(*ptr);
	*ptr = 0;
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

char *str_join(char *buf, char *add, int to_free)
{
	char	*newbuf;
	int		len;

	len = (buf) ? strlen(buf) : 0;
	newbuf = calloc(sizeof(*newbuf), (len + strlen(add) + 1));
	if (!newbuf)
		return (0);
	if (buf)
		strcat(newbuf, buf);
	if (to_free)
		ft_free((void **) &buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	ft_prepare_select(t_serv *serv) {
	int	i;

	FD_ZERO(&serv->reads);
	FD_ZERO(&serv->writes);
	FD_ZERO(&serv->errors);
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

void	ft_shrink(t_serv *serv) {
	t_fd	tab[FD_SETSIZE];
	int		i, j;

	i = 0;
	j = 0;
	bzero(tab, FD_SETSIZE);
	while (i < serv->nfds) {
		if (serv->fds[i++].fd > 0)
			tab[j++] = serv->fds[i - 1];
		else {
			ft_free((void **) &serv->fds[i - 1].send);
			ft_free((void **) &serv->fds[i - 1].recv);
		}
	}
	bzero(serv->fds, FD_SETSIZE * sizeof(t_fd));
	serv->nfds = j;
	while (--j >= 0)
		serv->fds[j] = tab[j];
}


int	ft_error(t_serv *serv, const char *str) {
	for (int i = 0; serv && i < serv->nfds; i++) {
		free(serv->fds[i].send);
		free(serv->fds[i].recv);
	}
	write(2, str, strlen(str));
	return (1);
}

/*
 * sends 'str' to all the clients except for 'fd'
 * if 'fd' is NULL all the clients will receive the message
 * if 'str' is NULL the does nothing
*/
void	ft_send_all(t_serv *serv, char *str, int fd) {
	if (!str)
		return ;
	for (int i = 0; i < serv->nfds; i++) {
		if (serv->fds[i].fd == fd)
			continue ;
		serv->fds[i].send = str_join(serv->fds[i].send, str, 1);
	}
}

void	ft_send_message(t_serv *serv, int i) {
	char	buff[BUFFSIZE];
	char	*tmp;
	char	*nl;
	int		ret;

	tmp = 0;
	nl = serv->fds[i].recv;
	while (nl && *nl) {
		ret = extract_message(&nl, &tmp);
		if (!ret)
			break ;
		bzero(buff, BUFFSIZE);
		sprintf(buff, "client %d: %s", serv->fds[i].id, tmp);
		ft_send_all(serv, buff, serv->fds[i].fd);
	}
	if (nl && *nl) {
		bzero(buff, BUFFSIZE);
		sprintf(buff, "client %d: %s", serv->fds[i].id, nl);
		ft_send_all(serv, buff, serv->fds[i].fd);
	}
	ft_free((void **) &nl);
	printf("message sent: %s>", serv->fds[i].recv);
	printf("message to send: %s>", serv->fds[i].send);
	ft_free((void **) &serv->fds[i].recv);
}

int	main(int ac, char **av) {
	char	buff[BUFFSIZE];
	t_serv	serv;
	int		ret;
	int		tmp;
	int		opt;
	int		n;

	if (ac != 2)
		return (ft_error(0, "Wrong number of arguments\n"));
	memset((void *)&serv, 0, sizeof(t_serv));
	serv.port = atoi(av[1]);
	// socket
	if ((serv.fds[0].fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return (ft_error(0, "Fatal error\n"));

	opt = 1;
	setsockopt(serv.fds[0].fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int));

	serv.server_addr.sin_family = AF_INET;
	serv.server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.server_addr.sin_port = htons((unsigned short) serv.port);

	// bind
	if (bind(serv.fds[0].fd, (struct sockaddr *)&serv.server_addr, sizeof(serv.server_addr)) < 0)
		return (ft_error(0, "Fatal error\n"));
	// listen
	if (listen(serv.fds[0].fd, FD_SETSIZE) < 0)
		return (ft_error(0, "Fatal error\n"));

	serv.fds[0].id = -1;
	serv.lid = 0;
	serv.nfds = 1;
	serv.bfd = serv.fds[0].fd;

	while (42) {
		ft_shrink(&serv);
		ft_prepare_select(&serv);
		/*
		 * select
		*/
		if ((ret = select(serv.bfd + 1, &serv.reads, &serv.writes, &serv.errors, &serv.tm)) < 0)
			return (ft_error(&serv, "Fatal error\n"));
		/*
		 * accept
		*/
		if (FD_ISSET(serv.fds[0].fd, &serv.reads)) {
			opt = sizeof(serv.client_addr);
			if ((tmp = accept(serv.fds[0].fd, (struct sockaddr *)&serv.client_addr, &opt)) > 0) {
				serv.fds[serv.nfds].fd = tmp;
				serv.fds[serv.nfds].id = serv.lid++;
				serv.bfd = (tmp > serv.bfd) ? tmp : serv.bfd;
				bzero(buff, BUFFSIZE);
				sprintf(buff, "server: client %d just arrived\n", serv.fds[serv.nfds].id);
				ft_send_all(&serv, buff, tmp);
				serv.nfds++;
			}
		}

		for (int i = 1; i < serv.nfds; i++) {
			n = 0;
			/*
			 * recv
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
//				bzero(serv.fds[i].recv, BUFFSIZE);
				do {
					bzero(buff, BUFFSIZE);
					n = recv(serv.fds[i].fd, buff, BUFFSIZE, MSG_DONTWAIT);
					if (!n) {
						ft_free((void **) &serv.fds[i].recv);
						break ;
					}
					printf("receiving: %s>", buff);
					serv.fds[i].recv = str_join(serv.fds[i].recv, buff, 1);
				} while (n == BUFFSIZE);
				printf("\nreceiving from %d: %s\n", serv.fds[i].id, serv.fds[i].recv);
				/*
				 * close connection
				*/
				if (!n && !serv.fds[i].recv) {
					serv.fds[i].fd = -1;
					bzero(buff, BUFFSIZE);
					sprintf(buff, "server: client %d just left\n", serv.fds[i].id);
					ft_send_all(&serv, buff, serv.fds[i].fd);
				}
				else if (n < 0) {
					printf("Error: 'recv' failed on %d\n", serv.fds[i].id);
				}
				/*
				 * send message received to all the clients
				*/
				else {
					ft_send_message(&serv, i);
				}
			}
			/*
			 * send
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.writes) && serv.fds[i].send) {
				n = send(serv.fds[i].fd, serv.fds[i].send, strlen(serv.fds[i].send), MSG_DONTWAIT);
				ft_free((void **) &serv.fds[i].send);
				if (n < 0) {
					serv.fds[i].fd = -1;
					printf("Error: 'send' failed\n");
				}
			}
			if (FD_ISSET(serv.fds[i].fd, &serv.errors))
				printf("ERROR: %d\n", serv.fds[i].id);
		}
	}
}


