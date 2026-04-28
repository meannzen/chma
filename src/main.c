#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 6379
#define  MAX_SIZE 1024
#define BACKLOG 10

void handle_connection(int fd) {
	char buff[MAX_SIZE];
	ssize_t n = 0;
	n = recv(fd, buff, MAX_SIZE -1, 0);
	if(n == -1) {
		perror("Failed to read socket");
		return;
	}
	printf("buff %s\n", buff);
	
}

int main() {
	struct sockaddr_in server_addr, client_addr;
	socklen_t addrlen = sizeof(server_addr);
	int opt = 1;
	int server_fd, new_socket;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_fd == -1) {
		perror("socket failed");
		return 1;
	}
	// trying to resuse connection
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt , sizeof(opt)) < 0) {
		perror("setsocketopt");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if(bind(server_fd, (struct sockaddr *)&server_addr, addrlen) < 0) {
		perror("bind failed");
		return 1;
	}

	if(listen(server_fd, BACKLOG) != 0) {
		perror("Listen failed");
		return 1;
	}

	if((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
		perror("Accept failed");
		return 1;
	}

	handle_connection(new_socket);
	close(server_fd);
	close(new_socket);

	return 0;
}
