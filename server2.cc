#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>
#define PORT 5001

int main(int argc, const char* argv[]) {

  // SERVER
  // assign IP, PORT
  struct sockaddr_in s_addr;
  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sin_family = AF_INET;  // use IPv4 not IPv6
  s_addr.sin_addr.s_addr =
      inet_addr("127.0.0.1");  // INADDR_ANY any address used to socket service
  s_addr.sin_port = htons(PORT);

  // creat socket
  int socket_server = socket(AF_INET, SOCK_STREAM, 0);  // TCP

  if (socket_server == -1) {
    std::cerr << "Not able to create socket!" << std::endl;
    return -1;
  }

  // Binding
  bind(socket_server, (struct sockaddr*)&s_addr, sizeof(s_addr));

  // Listening
  listen(socket_server, 10);

  // Log
  std::stringstream ss;
  ss << PORT;  // put data PORT in obj ss
  std::cout << "[Server] Listening on port " << ss.str() << std::endl;

  char buff[1024] = "alalalal";
  // memset(buff, 0, sizeof(buff));

  time_t ticks;

  // while waiting for client
  while (true) {
    // Accept connection from clients
    int conn_fd = accept(socket_server, (struct sockaddr*)NULL, NULL);
    std::cout << "[Server] Client successfully connected." << std::endl;
    ticks = time(NULL);
    // sprintf(buff, "Server reply %s", ctime(&ticks));
    write(conn_fd, buff, strlen(buff));
    close(conn_fd);
  }
  close(socket_server);
  return 0;
}
