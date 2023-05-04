#include <arpa/inet.h>  // used to inet
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>  // read, write
#include <iostream>
#include <sstream>  // handled string
#include <string>

#define PORT 9000

int main(int argc, const char* argv[]) {

  // SERVER
  // assign IP, PORT
  struct sockaddr_in s_addr = {.sin_family = AF_INET,  // use IPv4 not IPv6
                               .sin_addr.s_addr = inet_addr("127.0.0.1"),
                               .sin_port = htons(PORT)};

  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));

  // creat socket
  int socket_server = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  if (socket_server == -1) {
    std::cerr << "Not able to create socket!" << std::endl;
    return -1;
  }

  // Connect
  if (connect(socket_server, (struct sockaddr*)&s_addr, sizeof(s_addr)) != 0) {
    std::cerr << "Connection with the server failed...\n";
  }
  std::cout << "[Client] Connected to the server..\n";

  read(socket_server, buffer, sizeof(buffer) - 1);
  std::cout << "[Client] Data from server: " << buffer << std::endl;

  int n;
  for (;;) {
    memset(buffer, 0, sizeof(buffer));
    std::cout << "Enter the string : ";
    n = 0;
    while ((buffer[n++] = getchar()) != '\n')
      ;
    write(socket_server, buffer, sizeof(buffer));
    if ((strncmp(buffer, "exit", 4)) == 0) {
      std::cout << "Client Exit...\n";
      break;
    }
  }

  close(socket_server);
  return 0;
}
