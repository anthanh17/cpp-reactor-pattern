#include <arpa/inet.h>  // inet
#include <netdb.h>      // NI_MAXSERV, NI_MAXHOST
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>  // read, write, close
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#define PORT 9000

int main(int argc, const char *argv[]) {
  // create a master socket
  int master_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (master_socket == 0) {
    std::cerr << ("[Server] Socket failed!");
    exit(EXIT_FAILURE);
  }

  // set master socket to allow multiple connections ,
  // this is just a good habit, it will work without this
  int opt = 1;
  if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt)) < 0) {
    std::cerr << "[Server] Setsockopt";
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in address;
  // type of socket created
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // Bind
  if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    std::cerr << "[Server] Bind failed!";
    exit(EXIT_FAILURE);
  }

  // Listen
  if (listen(master_socket, 3) < 0) {
    std::cerr << "[Server] listen failed!";
    exit(EXIT_FAILURE);
  }

  std::cout << "[Server] Listening on Port " << PORT << std::endl;

  // accept the incoming connection
  int addrlen = sizeof(address);
  std::cout << "Waiting for connections ...\n";

  char buffer[1025];  // data buffer of 1K

  int value_read;
  int sd;
  int max_sd;

  // set of socket descriptors
  fd_set readfds;

  // a message
  char message[1024] = "hello i am server select";

  // initialise all client_socket[] to 0 so not checked
  int max_clients = 3;
  int client_socket[max_clients];
  for (int i = 0; i < max_clients; i++) {
    client_socket[i] = 0;
  }

  char data_recv[1024];

  struct timeval timeout;
  // Nonblocking
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  while (1) {
    // clear the socket set
    FD_ZERO(&readfds);

    // add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    // add child sockets to set
    for (int i = 0; i < max_clients; i++) {
      // socket descriptor
      sd = client_socket[i];

      // if valid socket descriptor then add to read list
      if (sd > 0) FD_SET(sd, &readfds);

      // highest file descriptor number, need it for the select function
      if (sd > max_sd) max_sd = sd;
    }

    // wait for an activity on one of the sockets , timeout is NULL ,
    // so wait indefinitely
    std::cout << "[LOG] before select \n";
    int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
    std::cout << "[LOG] after select \n";

    if ((activity < 0) && (errno != EINTR)) {
      std::cerr << "select error\n";
    }

    // Server handler
    if (FD_ISSET(master_socket, &readfds)) {
      int new_socket = accept(master_socket, (struct sockaddr *)&address,
                              (socklen_t *)&addrlen);
      std::cout << "[LOG] after accept \n";
      if (new_socket < 0) {
        std::cerr << "accept error\n";
        exit(EXIT_FAILURE);
      }

      // inform user of socket number - used in send and receive commands
      std::cout << "[+] New connection , socket fd: " << new_socket
                << " - ip: " << inet_ntoa(address.sin_addr)
                << " - port: " << ntohs(address.sin_port) << std::endl;

      // send new connection greeting message
      if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
        std::cerr << "send";
      }

      std::cout << "[Server] Message sent successfully\n";

      // add new socket to array of sockets
      for (int i = 0; i < max_clients; i++) {
        // if position is empty
        if (client_socket[i] == 0) {
          client_socket[i] = new_socket;
          std::cout << "Adding to list of sockets as: " << i << std::endl;

          break;
        }
      }
    }

    // Client Handler - loop client list
    for (int i = 0; i < max_clients; i++) {
      sd = client_socket[i];

      if (FD_ISSET(sd, &readfds)) {
        // Check if it was for closing , and also read the incoming message
        if ((value_read = read(sd, buffer, 1024)) == 0) {
          // Somebody disconnected , get his details and print
          getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
          std::cout << "Host disconnected ip " << inet_ntoa(address.sin_addr)
                    << " port " << ntohs(address.sin_port) << std::endl;

          // Close the socket and mark as 0 in list for reuse
          close(sd);
          client_socket[i] = 0;
        } else {
          std::cout << "[Client " << i << "]: " << buffer;
          sleep(1);
          // server send message to client
          // buffer[value_read] = '\0';
          // send(sd, buffer, strlen(buffer), 0);
        }
      }
    }
  }

  return 0;
}
