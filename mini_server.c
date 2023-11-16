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

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	len = (buf) ? strlen(buf) : 0;
	newbuf = calloc(sizeof(*newbuf), (len + strlen(add) + 1));
	if (!newbuf)
		return (0);
	if (buf)
		strcat(newbuf, buf);
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
	while (i < serv->nfds)
		if (serv->fds[i++].fd > 0)
			tab[j++] = serv->fds[i - 1];
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

int	main(int ac, char **av) {
	char	*buff;
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
				for (int i = 1; i < serv.nfds; i++) {
					buff = (char *) calloc(BUFFSIZE, sizeof(char));
					if (!buff)
						return (ft_error(&serv, "Fatal error\n"));
					sprintf(buff, "server: client %d just arrived\n", serv.fds[serv.nfds].id);
					serv.fds[i].send = str_join(serv.fds[i].send, buff);
					printf("%d receives: %s\n", serv.fds[i].id, serv.fds[i].send);
				}
				serv.nfds++;
			}
		}

		for (int i = 1; i < serv.nfds; i++) {
			/*
			 * recv
			*/
			if (FD_ISSET(serv.fds[i].fd, &serv.reads)) {
				
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
				;
		}
	}
}


