#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"

#if defined(__linux__)

#include <sys/epoll.h>

struct event_loop {
	int fd;
};

struct event_loop *event_loop_create(void)
{
	struct event_loop *l = malloc(sizeof(*l));
	if (!l)
		return NULL;
	l->fd = epoll_create1(0);
	if (l->fd == -1) {
		free(l);
		return NULL;
	}
	return l;
}

int event_add(struct event_loop *l, int fd, void *data)
{
	struct epoll_event ev = {
		.events = EPOLLIN | EPOLLRDHUP,
		.data.ptr = data,
	};
	return epoll_ctl(l->fd, EPOLL_CTL_ADD, fd, &ev);
}

int event_del(struct event_loop *l, int fd)
{
	return epoll_ctl(l->fd, EPOLL_CTL_DEL, fd, NULL);
}

int event_wait(struct event_loop *l, struct event *out, int max)
{
	struct epoll_event evs[max];
	int n;

	do {
		n = epoll_wait(l->fd, evs, max, -1);
	} while (n == -1 && errno == EINTR);
	if (n == -1)
		return -1;

	for (int i = 0; i < n; i++) {
		out[i].fd = -1;
		out[i].data = evs[i].data.ptr;
		out[i].readable = (evs[i].events & EPOLLIN) != 0;
		out[i].hangup =
			(evs[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) != 0;
	}
	return n;
}

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__)

#include <sys/event.h>
#include <sys/types.h>

struct event_loop {
	int fd;
};

struct event_loop *event_loop_create(void)
{
	struct event_loop *l = malloc(sizeof(*l));
	if (!l)
		return NULL;
	l->fd = kqueue();
	if (l->fd == -1) {
		free(l);
		return NULL;
	}
	return l;
}

int event_add(struct event_loop *l, int fd, void *data)
{
	struct kevent ch;
	EV_SET(&ch, fd, EVFILT_READ, EV_ADD, 0, 0, data);
	return kevent(l->fd, &ch, 1, NULL, 0, NULL);
}

int event_del(struct event_loop *l, int fd)
{
	struct kevent ch;
	EV_SET(&ch, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	return kevent(l->fd, &ch, 1, NULL, 0, NULL);
}

int event_wait(struct event_loop *l, struct event *out, int max)
{
	struct kevent evs[max];
	int n;

	do {
		n = kevent(l->fd, NULL, 0, evs, max, NULL);
	} while (n == -1 && errno == EINTR);
	if (n == -1)
		return -1;

	for (int i = 0; i < n; i++) {
		out[i].fd = (int)evs[i].ident;
		out[i].data = evs[i].udata;
		out[i].readable = evs[i].filter == EVFILT_READ;
		out[i].hangup = (evs[i].flags & (EV_EOF | EV_ERROR)) != 0;
	}
	return n;
}

#else
#error "no supported event backend"
#endif

void event_loop_destroy(struct event_loop *l)
{
	if (!l)
		return;
	close(l->fd);
	free(l);
}
