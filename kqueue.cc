
#include <arpa/inet.h>  // inet
#include <err.h>
#include <netdb.h>  // NI_MAXSERV, NI_MAXHOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>  // read, write, close
#include <ctime>
#include <iostream>

#define SERVER_PORT 9000
#define MAX_EVENTS 32

int CreateSocketAndListen() {
  int local_s = socket(AF_INET, SOCK_STREAM, 0);
  if (local_s < 0) {
    std::cerr << "Socket() failed";
    exit(-1);
  }
  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(local_s, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) <
      0) {
    std::cerr << "Setsockopt() failed";
    close(local_s);
    exit(-1);
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);
  if (bind(local_s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Bind() failed";
    close(local_s);
    exit(-1);
  }

  // Listen back log
  if (listen(local_s, 32) < 0) {
    std::cerr << "listen() failed";
    close(local_s);
    exit(-1);
  }
  std::cout << "[Server] Listening on Port " << SERVER_PORT << std::endl;
  std::cout << "Waiting for connections ...\n";

  return local_s;
}

// API connection
#define NUM_CLIENTS 10
int clients_fd[NUM_CLIENTS];

int GetConnectionIndex(int fd) {
  for (int i = 0; i < NUM_CLIENTS; i++)
    if (clients_fd[i] == fd) return i;
  return -1;
}

int AddConnection(int fd) {
  if (fd < 1) return -1;
  // get fd = = in client list
  int index = GetConnectionIndex(0);
  if (index == -1) {
    std::cerr << "Index failed, there are no connections in the array!";
    return -1;
  }
  // add fd to first element = 0
  clients_fd[index] = fd;
  return 0;
}

int RemoveConnection(int fd) {
  if (fd < 1) return -1;
  int index = GetConnectionIndex(fd);
  if (index == -1) {
    std::cerr << "Index failed, there are no connections in the array!";
    return -1;
  }
  clients_fd[index] = 0;
  return close(fd);
}

void ServerSendWelcomeMsg(int client_fd) {
  char msg[80];
  sprintf(msg, "welcome! you are client #%d!\n", GetConnectionIndex(client_fd));
  send(client_fd, msg, strlen(msg), 0);
}

void ReceiveMessages(int server_fd) {
  char buf[256];
  int bytes_read = recv(server_fd, buf, sizeof(buf) - 1, 0);
  buf[bytes_read] = 0;
  printf("client #%d: %s", GetConnectionIndex(server_fd), buf);
  fflush(stdout);
}

void EventLoop(int kq, int local_s) {
  // event want to monitor
  struct kevent evSet;
  // events that were triggered
  struct kevent evList[MAX_EVENTS];

  struct sockaddr_in addr;
  int socklen = sizeof(addr);

  while (1) {
    int num_events = kevent(kq, NULL, 0, evList, MAX_EVENTS, NULL);
    for (int i = 0; i < num_events; i++) {
      // READ
      if (evList[i].filter == EVFILT_READ) {
        // receive new connection
        if (evList[i].ident == local_s) {
          int fd = accept(evList[i].ident, (struct sockaddr *)&addr,
                          (socklen_t *)&socklen);
          if (fd < 1) {
            std::cerr << "accept failed";
            close(fd);
            exit(-1);
          }
          // add conection to array
          if (AddConnection(fd) == 0) {
            EV_SET(&evSet, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
            kevent(kq, &evSet, 1, NULL, 0, NULL);
            ServerSendWelcomeMsg(fd);
          } else {
            printf("Add failed connection.\n");
            close(fd);
          }
        }  // client disconnected
        else if (evList[i].flags & EV_EOF) {
          int fd = evList[i].ident;
          printf("client #%d disconnected.\n", GetConnectionIndex(fd));
          EV_SET(&evSet, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
          kevent(kq, &evSet, 1, NULL, 0, NULL);
          RemoveConnection(fd);
        }  // read message from client
        else {
          ReceiveMessages(evList[i].ident);
        }
      } else if (evList[i].filter == EVFILT_WRITE) {
        std::cout << "write!";
      } else {
        std::cout << "nothing!";
      }
    }
  }
}

int main(int argc, const char *argv[]) {
  int server_fd = CreateSocketAndListen();

  // The kqueue holds all the events we are interested in.
  int kq = kqueue();
  if (kq == -1) std::cerr << "kqueue() failed";
  // add sock server to queue monitor
  struct kevent evSet;
  EV_SET(&evSet, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
  kevent(kq, &evSet, 1, NULL, 0, NULL);
  // event loop
  EventLoop(kq, server_fd);
  return 0;
}
