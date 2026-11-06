#ifndef COMMAND_H
#define COMMAND_H

#include "parser.h"
#include "client.h"

typedef void (*cmd_fn)(struct client *c , const struct command *cmd) ;

struct cmd_entry {
    const char *name;
    int arity;
    cmd_fn fn;
};
void dispatch(struct client *client, const struct command *cmd);

#endif
