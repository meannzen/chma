#include "command.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

#include "client.h"
#include "parser.h"
#include "reply.h"
#include "store.h"

static void cmd_ping(struct client *c, const struct command *cmd);
static void cmd_echo(struct client *c, const struct command *cmd);
static void cmd_set(struct client *c, const struct command *cmd);
static void cmd_get(struct client *c, const struct command *cmd);
static void cmd_rpush(struct client *c , const struct command *cmd);

static const struct cmd_entry table[] = {
	{ "PING", -1, cmd_ping },
	{ "ECHO", 2, cmd_echo },
	{ "SET", -3, cmd_set },
	{ "GET", 2, cmd_get },
    {"RPUSH", -3, cmd_rpush},
};

static int name_eq(const char *s, size_t len, const char *name)
{
	return strlen(name) == len && strncasecmp(s, name, len) == 0;
}

void dispatch(struct client *client, const struct command *cmd)
{
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		const struct cmd_entry *entry = &table[i];

		if (!name_eq(cmd->argv[0], cmd->argl[0], entry->name))
			continue;

		int n = cmd->argc;
		if ((entry->arity > 0 && n != entry->arity) ||
		    (entry->arity < 0 && n < -entry->arity)) {
			reply_error(client, "ERR wrong number of arguments");
			return;
		}

		entry->fn(client, cmd);
		return;
	}

	reply_error(client, "ERR unknown command");
}

static void cmd_ping(struct client *c, const struct command *cmd)
{
	if (cmd->argc == 1)
		reply_simple(c, "PONG");
	else
		reply_bulk(c, cmd->argv[1], cmd->argl[1]);
}

static void cmd_echo(struct client *c, const struct command *cmd)
{
	reply_bulk(c, cmd->argv[1], cmd->argl[1]);
}

static int parse_ll(const char *s, size_t len, long long *out)
{
	long long v = 0;

	if (len == 0 || len > 18)
		return -1;
	for (size_t i = 0; i < len; i++) {
		if (s[i] < '0' || s[i] > '9')
			return -1;
		v = v * 10 + (s[i] - '0');
	}
	*out = v;
	return 0;
}

static void cmd_set(struct client *c, const struct command *cmd)
{
	long long expire_at = 0;

	if (cmd->argc != 3 && cmd->argc != 5) {
		reply_error(c, "ERR syntax error");
		return;
	}

	if (cmd->argc == 5) {
		long long mult;
		long long v;
		long long now;

		if (name_eq(cmd->argv[3], cmd->argl[3], "PX")) {
			mult = 1;
		} else if (name_eq(cmd->argv[3], cmd->argl[3], "EX")) {
			mult = 1000;
		} else {
			reply_error(c, "ERR syntax error");
			return;
		}

		if (parse_ll(cmd->argv[4], cmd->argl[4], &v) < 0) {
			reply_error(c, "ERR value is not an integer or out of range");
			return;
		}

		now = store_now_ms();
		if (v <= 0 || v > (LLONG_MAX - now) / mult) {
			reply_error(c, "ERR invalid expire time in 'set' command");
			return;
		}
		expire_at = now + v * mult;
	}

	if (store_set(c->store, cmd->argv[1], cmd->argl[1], cmd->argv[2],
		      cmd->argl[2], expire_at) < 0) {
		reply_error(c, "ERR out of memory");
		return;
	}
	reply_simple(c, "OK");
}

static void cmd_get(struct client *c, const struct command *cmd)
{
	const struct entry *e = store_get(c->store, cmd->argv[1], cmd->argl[1]);

	if (!e)
		reply_null(c);
	else if (e->list)
		reply_error(c, "WRONGTYPE Operation against a key holding the "
			       "wrong kind of value");
	else
		reply_bulk(c, e->val, e->val_len);
}


static void cmd_rpush(struct client *c, const struct command *cmd)
{
	size_t new_len;
	int rc = store_rpush(c->store, cmd->argv[1], cmd->argl[1],
			     &cmd->argv[2], &cmd->argl[2],
			     (size_t)(cmd->argc - 2), &new_len);

	if (rc == STORE_WRONGTYPE) {
		reply_error(c, "WRONGTYPE Operation against a key holding the "
			       "wrong kind of value");
		return;
	}
	if (rc < 0) {
		reply_error(c, "ERR out of memory");
		return;
	}
	reply_int(c, (int)new_len);
}
