#include "command.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

#include "client.h"
#include "parser.h"
#include "reply.h"

static void cmd_ping(struct client *c, const struct command *cmd);
static void cmd_echo(struct client *c, const struct command *cmd);

static const struct cmd_entry table[] = {
	{ "PING", -1, cmd_ping },
	{ "ECHO", 2, cmd_echo },
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
