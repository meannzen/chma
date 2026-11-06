#ifndef EVENT_H
#define EVENT_H

struct event_loop;

struct event {
	int fd;
	void *data;
	int readable;
	int hangup;
};

struct event_loop *event_loop_create(void);
int event_add(struct event_loop *l, int fd, void *data);
int event_del(struct event_loop *l, int fd);
int event_wait(struct event_loop *l, struct event *out, int max,
	       int timeout_ms);
void event_loop_destroy(struct event_loop *l);

#endif
