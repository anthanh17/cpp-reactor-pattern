
#include <arpa/inet.h>  // inet
#include <netdb.h>      // NI_MAXSERV, NI_MAXHOST
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>  // read, write, close
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#define SERVER_PORT 9000

int main(int argc, const char *argv[]) {
  int compress_array = 0;

  // create socket
  int sock_server = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_server < 0) {
    std::cerr << "socket() failed";
    exit(-1);
  }

  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt)) < 0) {
    std::cerr << "setsockopt() failed";
    close(sock_server);
    exit(-1);
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);

  if (bind(sock_server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "bind() failed";
    close(sock_server);
    exit(-1);
  }

  // Listen back log
  if (listen(sock_server, 32) < 0) {
    std::cerr << "listen() failed";
    close(sock_server);
    exit(-1);
  }
  std::cout << "[Server] Listening on Port " << SERVER_PORT << std::endl;
  std::cout << "Waiting for connections ...\n";

  // Initialize the pollfd structure
  struct pollfd fds[200];
  memset(fds, 0, sizeof(fds));

  // Set up the initial listening socket
  fds[0].fd = sock_server;
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  int timeout = -1;  // 0 -  nonblocking

  int nfds = 1, current_size = 0;

  // Loop waiting for incoming connects or for incoming data on any of the
  // connected sockets.
  bool run_server = true;
  int close_conn;
  char recv_buffer[80];
  while (run_server) {
    std::cout << "[LOG] before poll \n";
    int ready_fds = poll(fds, nfds, timeout);
    if (ready_fds < 0) {
      std::cerr << "poll() failed";
      continue;
    } else if (ready_fds == 0) {
      continue;
    }
    std::cout << "[LOG] after select \n";

    current_size = nfds;
    for (int i = 0; i < current_size; i++) {
      if ((fds[i].revents & POLLIN) != POLLIN) continue;

      // if SERVER request for new connection
      if (fds[i].fd == sock_server) {
        // Listening descriptor is readable.
        std::cout << "Listening socket is readable\n";

        int new_sd = accept(sock_server, NULL, NULL);
        if (new_sd < 0) {
          std::cerr << "accept() failed";
          close(new_sd);
          continue;
        }
        std::cout << "New incoming connection - " << new_sd << std::endl;
        // Add the new incoming connection to the pollfd structure
        fds[nfds].fd = new_sd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;

        char message[1024] = "Hello i am select server!";
        if (send(new_sd, message, strlen(message), 0) != strlen(message)) {
          std::cerr << "send";
        }
        std::cout << "[Server] Message sent successfully\n";

      } else {  // CLIENT some client is sending data
        std::cout << "Descriptor " << fds[i].fd << " is readable\n";
        close_conn = 0;

        int bytes_read = read(fds[i].fd, recv_buffer, sizeof(recv_buffer));
        if (bytes_read < 0) {
          std::cerr << "  read() failed";
          close_conn = 1;
          continue;
        }
        // Check to see if the connection has been closed by the client
        if (bytes_read == 0) {
          std::cout << "Connection closed\n";
          close_conn = 1;
        }
        // Data was received
        std::cout << " bytes received: " << recv_buffer;

        if (close_conn) {
          close(fds[i].fd);
          fds[i].fd = -1;
          compress_array = 1;
        }
      }
    }

    /* If the compress_array flag was turned on, we need       */
    /* to squeeze together the array and decrement the number  */
    /* of file descriptors. We do not need to move back the    */
    /* events and revents fields because the events will always*/
    /* be POLLIN in this case, and revents is output.          */
    if (compress_array) {
      compress_array = 0;
      for (int i = 0; i < nfds; i++) {
        if (fds[i].fd == -1) {
          for (int j = i; j < nfds; j++) {
            fds[j].fd = fds[j + 1].fd;
          }
          i--;
          nfds--;
        }
      }
    }
  }

  // Clean up all of the sockets that are open
  for (int i = 0; i < nfds; i++) {
    if (fds[i].fd >= 0) close(fds[i].fd);
  }
}
