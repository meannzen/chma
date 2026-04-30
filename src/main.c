#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

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

void handle_connection(int fd, struct sockaddr_in *client_addr) {
  char buff[MAX_SIZE];
  ssize_t n = 0;
  Cursor *cursor = init_cursor();
  while((n = recv(fd, buff, MAX_SIZE - 1, 0)) > 0){
    if(n == 0) {
      char ipstr[INET_ADDRSTRLEN];
      int port = ntohs(client_addr->sin_port);
      inet_ntop(AF_INET, &client_addr->sin_addr, ipstr, sizeof(ipstr));
      printf("Client %s:%d Disconnection...\n", ipstr, port);
      exit(1);
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

  struct pollfd polls_fds[1];

  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  polls_fds[0].fd = server_fd;
  polls_fds[0].events = POLLIN;

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

  if (listen(server_fd, BACKLOG) < 0) {
      perror("Listen failed");
      return 1;
  }


  while (1) {
    int poll_count = poll(polls_fds, 1, -1);
    if(poll_count == -1) {
      perror("poll failed");
      exit(1);
    }

    if(polls_fds[0].revents & (POLLIN | POLLHUP)) {
      printf("ready to read data\n");
    }
    if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                             &addrlen)) < 0) {
      perror("Accept failed");
      return 1;
    }

    handle_connection(new_socket, &client_addr);
    close(new_socket);
  }

  close(server_fd);

  return 0;
}
