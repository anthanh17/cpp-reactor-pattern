
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
  int len, rc;
  int new_sd = -1;
  int desc_ready, end_server = 0, compress_array = 0;
  int close_conn;
  char buffer[80];

  // create socket
  int sock_server = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_server < 0) {
    std::cerr << "socket() failed";
    exit(-1);
  }

  // Allow socket descriptor to be reuseable
  int opt = 1;
  rc = setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                  sizeof(opt));
  if (rc < 0) {
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

  rc = bind(sock_server, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0) {
    std::cerr << "bind() failed";
    close(sock_server);
    exit(-1);
  }

  // Listen back log
  rc = listen(sock_server, 32);
  if (rc < 0) {
    std::cerr << "listen() failed";
    close(sock_server);
    exit(-1);
  }

  // Initialize the pollfd structure
  struct pollfd fds[200];
  memset(fds, 0, sizeof(fds));

  // Set up the initial listening socket
  fds[0].fd = sock_server;
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  int timeout = -1;  // 0 -  nonblocking

  int nfds = 1, current_size = 0, j;

  // Loop waiting for incoming connects or for incoming data on any of the
  // connected sockets.
  do {

    // Call poll() and wait 3 minutes for it to complete.
    std::cout << "Waiting on poll()...\n";
    rc = poll(fds, nfds, timeout);
    std::cout << "[LOG] after select \n";

    // Check to see if the poll call failed.
    if (rc < 0) {
      std::cerr << "poll() failed";
      break;
    }

    current_size = nfds;
    for (int i = 0; i < current_size; i++) {
      if (fds[i].revents == 0) continue;

      // If revents is not POLLIN, it's an unexpected result,
      // log and end the server.
      if (fds[i].revents != POLLIN) {
        std::cout << "Error! revents = " << fds[i].revents << std::endl;
        end_server = 1;
        break;
      }

      // if SERVER request for new connection
      if (fds[i].fd == sock_server) {
        // Listening descriptor is readable.
        std::cout << "Listening socket is readable\n";

        new_sd = accept(sock_server, NULL, NULL);
        if (new_sd < 0) {
          if (errno != EWOULDBLOCK) {
            std::cerr << "accept() failed";
            end_server = 1;
          }
          break;
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

        rc = read(fds[i].fd, buffer, sizeof(buffer));
        if (rc < 0) {
          if (errno != EWOULDBLOCK) {
            std::cerr << "  recv() failed";
            close_conn = 1;
          }
          break;
        }
        // Check to see if the connection has been closed by the client
        if (rc == 0) {
          std::cout << "Connection closed\n";
          close_conn = 1;
        }
        // Data was received
        len = rc;
        std::cout << len << " bytes received: " << buffer;

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
          for (j = i; j < nfds; j++) {
            fds[j].fd = fds[j + 1].fd;
          }
          i--;
          nfds--;
        }
      }
    }
  } while (end_server == 0);  // End of serving running.

  // Clean up all of the sockets that are open
  for (int i = 0; i < nfds; i++) {
    if (fds[i].fd >= 0) close(fds[i].fd);
  }
}
