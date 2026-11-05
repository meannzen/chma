#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 6379
#define MAX_SIZE 1024
#define BACKLOG 10

static const char PONG[] = "+PONG\r\n";

static void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	pfds[i] = pfds[*fd_count - 1];
	(*fd_count)--;
}

static void add_to_pfds(struct pollfd **pfds, int newfd, int *fd_count,
			int *fd_size)
{
	if (*fd_count == *fd_size) {
		int new_size = *fd_size * 2;
		struct pollfd *tmp = realloc(*pfds, sizeof(**pfds) * new_size);
		if (!tmp) {
			perror("realloc");
			close(newfd);
			return;
		}
		*pfds = tmp;
		*fd_size = new_size;
	}

	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;
	(*pfds)[*fd_count].revents = 0;
	(*fd_count)++;
}

static void handle_new_connection(int listener, int *fd_size, int *fd_count,
				  struct pollfd **pfds)
{
	int new_fd = accept(listener, NULL, NULL);
	if (new_fd == -1) {
		perror("accept");
		return;
	}
	add_to_pfds(pfds, new_fd, fd_count, fd_size);
}

static void handle_client(struct pollfd *pfds, int *i, int *fd_count)
{
	char buff[MAX_SIZE];
	int fd = pfds[*i].fd;
	ssize_t n = recv(fd, buff, sizeof(buff) - 1, 0);

	if (n <= 0) {
		close(fd);
		del_from_pfds(pfds, *i, fd_count);
		(*i)--;
		return;
	}
	buff[n] = '\0';

	for (char *p = buff; (p = strstr(p, "PING")) != NULL; p += 4) {
		if (send(fd, PONG, sizeof(PONG) - 1, 0) == -1) {
			perror("send");
			break;
		}
	}
}

int main(void)
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

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
	if (!pfds) {
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
				handle_new_connection(server_fd, &fd_size,
						      &fd_count, &pfds);
			else
				handle_client(pfds, &i, &fd_count);
		}
	}

	free(pfds);
	close(server_fd);
	return 0;
}
