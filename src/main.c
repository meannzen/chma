#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"
#include "command.h"
#include "event.h"
#include "parser.h"
#include "reply.h"
#include "store.h"

#define PORT 6379
#define BACKLOG 10

#define EAR_TICK_MS 400
#define EAR_LINES_BELOW 6

static const char *const EAR_FRAMES[] = {
	"/\\_/\\",
	"|\\_/\\",
	"/\\_/\\",
	"/\\_/|",
};
#define EAR_FRAME_COUNT (sizeof(EAR_FRAMES) / sizeof(EAR_FRAMES[0]))

static void banner_print(const char *ears)
{
	printf("\n"
	       "      %s\n"
	       "     ( o.o )\n"
	       "      > ^ <\n"
	       "\n"
	       "  meow! redis is listening on port %d\n"
	       "\n",
	       ears, PORT);
}

static void banner_redraw_ears(const char *ears)
{
	printf("\033[%dA\r\033[2K      %s\033[%dB\r", EAR_LINES_BELOW, ears,
	       EAR_LINES_BELOW);
}

static void drop_client(struct event_loop *loop, struct client *c)
{
	event_del(loop, c->fd);
	close(c->fd);
	free(c);
}

static void add_client(struct event_loop *loop, int newfd, struct store *store)
{
	struct client *c = malloc(sizeof(*c));
	if (!c)
		goto fail;
	client_init(c, newfd, store);

	if (event_add(loop, newfd, c) == -1) {
		free(c);
		goto fail;
	}
	return;

fail:
	perror("add_client");
	close(newfd);
}

static void handle_new_connection(struct event_loop *loop, int listener,
				  struct store *store)
{
	int new_fd = accept(listener, NULL, NULL);
	if (new_fd == -1) {
		perror("accept");
		return;
	}
	add_client(loop, new_fd, store);
}

static void handle_client(struct event_loop *loop, struct client *c)
{
	if (c->in_len == sizeof(c->in)) {
		drop_client(loop, c);
		return;
	}

	ssize_t n = recv(c->fd, c->in + c->in_len, sizeof(c->in) - c->in_len, 0);
	if (n <= 0) {
		drop_client(loop, c);
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
		drop_client(loop, c);
		return;
	}

	if (client_flush(c) < 0)
		drop_client(loop, c);
}

int main(void)
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	signal(SIGPIPE, SIG_IGN);

	int opt = 1;

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

	int animating = isatty(STDOUT_FILENO);
	size_t frame = 0;
	banner_print(EAR_FRAMES[0]);

	struct event_loop *loop = event_loop_create();
	struct store *store = store_create();
	if (!loop || !store) {
		perror("init");
		return 1;
	}
	if (event_add(loop, server_fd, NULL) == -1) {
		perror("event_add");
		return 1;
	}

	struct event evs[64];
	while (1) {
		int n = event_wait(loop, evs, 64, animating ? EAR_TICK_MS : -1);
		if (n == -1) {
			perror("event_wait failed");
			exit(1);
		}

		if (n == 0 && animating) {
			frame = (frame + 1) % EAR_FRAME_COUNT;
			banner_redraw_ears(EAR_FRAMES[frame]);
			continue;
		}

		for (int i = 0; i < n; i++) {
			if (!evs[i].readable && !evs[i].hangup)
				continue;

			if (!evs[i].data)
				handle_new_connection(loop, server_fd, store);
			else
				handle_client(loop, evs[i].data);
		}
	}

	store_destroy(store);
	event_loop_destroy(loop);
	close(server_fd);
	return 0;
}
