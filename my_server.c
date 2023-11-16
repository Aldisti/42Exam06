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

#define BUFFSIZE 1
#define SOCKADDR struct sockaddr_in

typedef struct s_fd {
	int		fd;
	int		id;
}	t_fd;

typedef struct s_serv {
	char			buff[BUFFSIZE];
	char			*send_buff;
	char			*recv_buff;
	int				fds[FD_SETSIZE]; // array to store all the fds
	int				nfds; // number of fds
	int				bfd; // biggest fd
	fd_set			reads;
	fd_set			writes;
	fd_set			errors;
	int				port;
	SOCKADDR		server_addr;
	SOCKADDR		client_addr;
	struct timeval	tm;
}	t_serv;

int	ft_printf(int fd, const char *str) {
	if (fd < 0)
		return (-1);
	return (write(fd, str, strlen(str)));
}

void	ft_prepare_select(t_serv *serv) {
	int	i;

	FD_ZERO(&serv->reads);
	FD_ZERO(&serv->writes);
	FD_ZERO(&serv->errors);
	i = 0;
	while (i < serv->nfds) {
		FD_SET(serv->fds[i], &serv->reads);
		FD_SET(serv->fds[i], &serv->writes);
		FD_SET(serv->fds[i], &serv->errors);
		i++;
	}
	serv->tm.tv_sec = 1;
	serv->tm.tv_usec = 0;
}

void	print_tab(const int *tab, int size) {
	printf("[");
	for (int i = 0; i < size; i++) {
		printf("%d", tab[i]);
		if (i < size - 1)
			printf(",");
	}
	printf("]\n");
}

void	ft_shrink(t_serv *serv) {
	int	tab[FD_SETSIZE];
	int i, j;

	i = 0;
	j = 0;
	bzero(tab, FD_SETSIZE);
	while (i < serv->nfds)
		if (serv->fds[i++] > 0)
			tab[j++] = serv->fds[i - 1];
	bzero(serv->fds, FD_SETSIZE);
	serv->nfds = j;
	while (--j >= 0)
		serv->fds[j] = tab[j];
}

void	ft_recive(t_serv *serv, int i) {
	int	n;

	while (42) {
		n = recv(serv->fds[i], serv->buff, BUFFSIZE, MSG_DONTWAIT);
		if (n < 0) {
			serv->fds[i] = -1;
			break ;
		}
		else if (n == 0)
			break ;
		serv->recv_buff = (char *) realloc(serv->recv_buff, n);
		serv->recv_buff = strcat(serv->recv_buff, serv->buff);
		printf("%d\n", n);
	}
	printf("received: |%s|\n", serv->recv_buff);

}

int	main(int ac, char **av) {
	t_serv	serv;
	int		ret;
	int		tmp;
	int		opt;
	int		n;

	if (ac != 2)
		return (ft_printf(2, "Error: usage: ./a.out <port>\n"));
	memset((void *)&serv, 0, sizeof(t_serv));
	serv.port = atoi(av[1]);
	if ((serv.fds[0] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return (ft_printf(2, "Error: 'socket' failed\n"));
	opt = 1;
	setsockopt(serv.fds[0], SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int));

	serv.server_addr.sin_family = AF_INET;
	serv.server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.server_addr.sin_port = htons((unsigned short) serv.port);

	if (bind(serv.fds[0], (struct sockaddr *)&serv.server_addr, sizeof(serv.server_addr)) < 0)
		return (ft_printf(2, "Error: 'bind' failed\n"));
	if (listen(serv.fds[0], FD_SETSIZE) < 0)
		return (ft_printf(2, "Error: 'listen' failed\n"));

	opt = sizeof(serv.client_addr);
	serv.nfds = 1;
	serv.bfd = serv.fds[0];
	ft_printf(1, "> while\n");

	while (42) {
		ft_shrink(&serv);
		ft_prepare_select(&serv);
		if ((ret = select(serv.bfd + 1, &serv.reads, &serv.writes, &serv.errors, &serv.tm)) < 0)
			return (ft_printf(2, "Error: 'select' failed\n"));
		if (FD_ISSET(serv.fds[0], &serv.reads)) {
			printf("> accept\n");
			if ((tmp = accept(serv.fds[0], (struct sockaddr *)&serv.client_addr, &opt)) < 0)
				ft_printf(2, "Error: 'accept' failed\n");
			else {
				serv.fds[serv.nfds++] = tmp;
				serv.bfd = tmp;
			}
		}
		for (int i = 1; i < serv.nfds; i++) {
			n = 0;
			if (FD_ISSET(serv.fds[i], &serv.reads))
				ft_recive(&serv, i);
			if (FD_ISSET(serv.fds[i], &serv.writes) && n > 0) {
				n = send(serv.fds[i], serv.buff, n, 0);
				if (n < 0) {
					ft_printf(2, "Error: 'send' failed\n");
					serv.fds[i] = -1;
				}
				else
					printf("sent %d bytes to %d\n", n, serv.fds[i]);
			}
			if (FD_ISSET(serv.fds[i], &serv.errors)) {
				printf("client %d ERROR\n", serv.fds[i]);
				serv.fds[i] = -1;
			}
		}
	}
}

