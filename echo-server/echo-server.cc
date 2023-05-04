#include <arpa/inet.h>  // inet
#include <netdb.h>      // NI_MAXSERV, NI_MAXHOST
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  // read, write, close
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#define PORT 5002

int main(int argc, const char* argv[]) {
  struct sockaddr_in server_addr;

  // assign IP, PORT for server
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;  // use IPv4
  server_addr.sin_addr.s_addr =
      inet_addr("127.0.0.1");  // IP loop back - localhost
  server_addr.sin_port = htons(PORT);

  // create socket
  int sock_server_fd = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  int option = 1;                                        // option connection
  setsockopt(sock_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option,
             sizeof(option));
  if (sock_server_fd == -1) {
    std::cerr << "Not able to create socket!" << std::endl;
    return -1;
  }
  std::cout << "[Server] Create socket successfully...\n";

  // Binding
  if ((bind(sock_server_fd, (struct sockaddr*)&server_addr,
            sizeof(server_addr))) != 0) {
    std::cerr << "Socket bind failed...\n";
    return -1;
  }
  std::cout << "[Server] Socket successfully binded..\n";

  // Server is ready to listen
  if ((listen(sock_server_fd, SOMAXCONN)) != 0) {
    std::cerr << "Listen failed...\n";
    return -1;
  }
  std::stringstream ss;
  ss << PORT;
  std::cout << "[Server] Listening on port " << ss.str() << std::endl;

  // while waiting for clients
  struct sockaddr_in client_addr;
  char buff[1024] = "Hello from server!";
  while (true) {
    // Accept connection the data packet from clients
    int sock_client_fd = accept(sock_server_fd, (struct sockaddr*)&client_addr,
                                (socklen_t*)sizeof(client_addr));
    std::cout << "[Server] Client successfully connected!" << std::endl;
    // current date/time based on current system
    time_t now = time(0);
    std::cout << "Time: " << ctime(&now);

    // Try to find out who is the client
    char host_client[NI_MAXHOST];
    char port_client[NI_MAXSERV];
    // clear - copy one character in object => fill all element object equal 0
    memset(host_client, 0, NI_MAXHOST);
    memset(port_client, 0, NI_MAXSERV);
    if (getnameinfo((sockaddr*)&client_addr, sizeof(client_addr), host_client,
                    NI_MAXHOST, port_client, NI_MAXSERV, 0) == 0) {
      std::cout << " --> " << host_client << " connected to port "
                << port_client << std::endl;
    } else {
      inet_ntop(AF_INET, &client_addr.sin_addr, host_client, NI_MAXHOST);
      std::cout << " --> " << host_client << " connected to port "
                << ntohs(client_addr.sin_port) << std::endl;
    }

    write(sock_client_fd, buff, strlen(buff));
    close(sock_client_fd);
  }

  close(sock_server_fd);
  return 0;
}
