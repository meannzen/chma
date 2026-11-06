#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "command.h"
#include "parser.h"
#include "reply.h"

#define PORT 6379
#define BACKLOG 10

static void drop_client(struct pollfd *pfds, struct client **clients, int *i,
			int *fd_count)
{
	int last = *fd_count - 1;

	close(pfds[*i].fd);
	free(clients[*i]);

	pfds[*i] = pfds[last];
	clients[*i] = clients[last];
	clients[last] = NULL;

	(*fd_count)--;
	(*i)--;
}

static void add_client(struct pollfd **pfds, struct client ***clients,
		       int newfd, int *fd_count, int *fd_size)
{
	if (*fd_count == *fd_size) {
		int new_size = *fd_size * 2;

		struct pollfd *p = realloc(*pfds, sizeof(**pfds) * new_size);
		if (!p)
			goto fail;
		*pfds = p;

		struct client **cl = realloc(*clients, sizeof(**clients) * new_size);
		if (!cl)
			goto fail;
		*clients = cl;

		for (int k = *fd_size; k < new_size; k++)
			cl[k] = NULL;
		*fd_size = new_size;
	}

	struct client *c = malloc(sizeof(*c));
	if (!c)
		goto fail;
	client_init(c, newfd);

	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;
	(*pfds)[*fd_count].revents = 0;
	(*clients)[*fd_count] = c;
	(*fd_count)++;
	return;

fail:
	perror("add_client");
	close(newfd);
}

static void handle_new_connection(int listener, struct pollfd **pfds,
				  struct client ***clients, int *fd_count,
				  int *fd_size)
{
	int new_fd = accept(listener, NULL, NULL);
	if (new_fd == -1) {
		perror("accept");
		return;
	}
	add_client(pfds, clients, new_fd, fd_count, fd_size);
}

static void handle_client(struct pollfd *pfds, struct client **clients, int *i,
			  int *fd_count)
{
	struct client *c = clients[*i];

	if (c->in_len == sizeof(c->in)) {
		drop_client(pfds, clients, i, fd_count);
		return;
	}

	ssize_t n = recv(c->fd, c->in + c->in_len, sizeof(c->in) - c->in_len, 0);
	if (n <= 0) {
		drop_client(pfds, clients, i, fd_count);
		return;
	}
	c->in_len += (size_t)n;

	struct command cmd;
	size_t used;
	enum parse_status st;
	while ((st = parse_command(c->in, c->in_len, &cmd, &used)) == PARSE_OK) {
		dispatch(c, &cmd);
		client_consume(c, used);
	}

	if (st == PARSE_ERROR) {
		reply_error(c, "ERR Protocol error");
		client_flush(c);
		drop_client(pfds, clients, i, fd_count);
		return;
	}

	if (client_flush(c) < 0)
		drop_client(pfds, clients, i, fd_count);
}

int main(void)
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	signal(SIGPIPE, SIG_IGN);

	int opt = 1;
	int fd_size = 5;
	int fd_count = 0;

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket failed");
		return 1;
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
	    0) {
		perror("setsockopt");
		return 1;
	}

	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(PORT),
	};

	if (bind(server_fd, (struct sockaddr *)&server_addr,
		 sizeof(server_addr)) < 0) {
		perror("bind failed");
		return 1;
	}

	if (listen(server_fd, BACKLOG) < 0) {
		perror("listen failed");
		return 1;
	}

	struct pollfd *pfds = malloc(sizeof(*pfds) * fd_size);
	struct client **clients = calloc(fd_size, sizeof(*clients));
	if (!pfds || !clients) {
		perror("malloc");
		return 1;
	}
	pfds[0].fd = server_fd;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;
	fd_count = 1;

	while (1) {
		if (poll(pfds, fd_count, -1) == -1) {
			perror("poll failed");
			exit(1);
		}

		for (int i = 0; i < fd_count; i++) {
			if (!(pfds[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;

			if (pfds[i].fd == server_fd)
				handle_new_connection(server_fd, &pfds, &clients,
						      &fd_count, &fd_size);
			else
				handle_client(pfds, clients, &i, &fd_count);
		}
	}

	free(pfds);
	free(clients);
	close(server_fd);
	return 0;
}
