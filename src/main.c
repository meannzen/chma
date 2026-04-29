#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 6379
#define MAX_SIZE 1024
#define BACKLOG 10


typedef struct  {
  char *buff;
  ssize_t len;
  ssize_t position;
} Cursor;

Cursor *init_cursor() {
  Cursor *c = malloc(sizeof(Cursor));
  if(!c) {
    perror("allocate memory");
    exit(1);
  }
  c->buff = NULL;
  c->len = 0;
  c->position = 0;
  return c;
}

int get_decimal(Cursor*cursor) {
  char value[5] = {};
  int i = 0;
  while (cursor->buff[cursor->position] !='\r' && cursor->buff[cursor->position + 1]!= '\n' ) {
    if(i < 5) {
      value[i] = cursor->buff[cursor->position++]; 
    }
    i++;
  }

  return atoi(value);
}

void parser(Cursor *cursor) {
  switch (cursor->buff[cursor->position]) {
    case '*':{    
      cursor->position++;
      int size = get_decimal(cursor);
      printf("array size %d\n", size);
      return;
    }
    case '+':
      printf("String\n");

      return;
    case ':':
      printf("Integer\n");
      return;
    case '$': 
      printf("Bulk string\n");
      return;
    default:
      printf("I Don't know\n");
      return;
  }
}

void handle_connection(int fd) {
  char buff[MAX_SIZE];
  ssize_t n = 0;
  Cursor *cursor = init_cursor();
  while((n = recv(fd, buff, MAX_SIZE - 1, 0)) !=-1){
    if(n == 0) {
      return;
    }

    char *str = malloc(n*sizeof(char));
    if(!str) {
      exit(1);
    }
    memcpy(str, buff, n);
    cursor->buff = str;
    cursor->len = n;
    parser(cursor);
    free(cursor->buff);
    const char *response = "+PONG\r\n";
    send(fd, response, strlen(response), 0);
  }

  free(cursor);
  
}

int main() {
  setbuf(stdout, NULL);
	setbuf(stderr, NULL);
  struct sockaddr_in server_addr, client_addr;
  socklen_t addrlen = sizeof(server_addr);
  int opt = 1;
  int server_fd, new_socket;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket failed");
    return 1;
  }
  // trying to resuse connection
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsocketopt");
    return 1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_addr, addrlen) < 0) {
    perror("bind failed");
    return 1;
  }

  while (1) {
    if (listen(server_fd, BACKLOG) != 0) {
      perror("Listen failed");
      return 1;
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                             &addrlen)) < 0) {
      perror("Accept failed");
      return 1;
    }

    handle_connection(new_socket);
    close(new_socket);
  }

  close(server_fd);

  return 0;
}
