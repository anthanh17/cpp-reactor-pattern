#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>

#define PORT 5000

int main(int argc, const char* argv[]) {

  // SERVER
  // assign IP, PORT
  struct sockaddr_in s_addr = {
      .sin_family = AF_INET,  // use IPv4 not IPv6
      .sin_addr.s_addr = inet_addr(
          "127.0.0.1"),  // INADDR_ANY any address used to socket service
      .sin_port = htons(PORT)};

  int option = 1;  // option connection
  int s_addr_size = sizeof(s_addr);

  // creat socket
  int socket_server = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option,
             sizeof(option));

  if (socket_server == -1) {
    std::cerr << "Not able to create socket!" << std::endl;
    return -1;
  }

  // CLIENT
  struct sockaddr_in c_addr;
  socklen_t c_addr_size = sizeof(c_addr);
  int socket_client;

  // Binding
  bind(socket_server, (struct sockaddr*)&s_addr, sizeof(s_addr));

  // Listening
  listen(socket_server, SOMAXCONN);

  // Print
  std::stringstream ss;
  ss << PORT;  // put data PORT in obj ss
  std::cout << "[Server] Listening on port " << ss.str() << std::endl;

  char buff[4096];
  int size_in_bytes_of_received_data;

  // while waiting for client
  while (true) {
    // Accept connection from clients
    std::cout << "annt" << ss.str() << std::endl;
    socket_client = accept(socket_server, (struct sockaddr*)&c_addr,
                           (socklen_t*)&c_addr_size);
    std::cout << "[Server] Client successfully connected." << std::endl;

    // Try to find out who is the client
    char host_client[NI_MAXHOST];
    char port_client[NI_MAXSERV];
    // clear - copy one character in object => fill all element object equal 0
    memset(host_client, 0, NI_MAXHOST);
    memset(port_client, 0, NI_MAXSERV);

    if (getnameinfo((sockaddr*)&c_addr, sizeof(c_addr), host_client, NI_MAXHOST,
                    port_client, NI_MAXSERV, 0) == 0) {
      std::cout << " --> " << host_client << "connected to port " << port_client
                << std::endl;
    } else {
      inet_ntop(AF_INET, &c_addr.sin_addr, host_client, NI_MAXHOST);
      std::cout << " --> " << host_client << "connected to port "
                << ntohs(c_addr.sin_port) << std::endl;
    }

    // Receive our data
    size_in_bytes_of_received_data = recv(socket_client, buff, 4096, 0);
    if (size_in_bytes_of_received_data == -1) {
      std::cerr << "Error receiving message.";
      break;
    } else if (size_in_bytes_of_received_data == 0) {
      std::cout << "Client disconnected" << std::endl;
      break;
    }
    send(socket_client, buff, size_in_bytes_of_received_data + 1, 0);

    std::cout << std::string(buff, 0, size_in_bytes_of_received_data)
              << std::endl;

    close(socket_client);
    // echo -en "testing data from client\0" | nc 127.0.0.1 5000
  }

  return 0;
}
