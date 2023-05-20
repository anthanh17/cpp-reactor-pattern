
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
    std::cerr << "[server] index failed, there are no connections in the array!";
    return -1;
  }
  // add fd to first element = 0
  clients_fd[index] = fd;
  return 0;
}

int RemoveConnection(int fd) {
  if (fd < 1)
    return -1;
  int index = GetConnectionIndex(fd);
  if (index == -1) {
    std::cerr << "[server] index failed, there are no connections in the array!";
    return -1;
  }
  clients_fd[index] = 0;
  return close(fd);
}

int CreateSocketAndListen() {
  int local_s = socket(AF_INET, SOCK_STREAM, 0);
  if (local_s < 0) {
    std::cerr << "[server] socket() failed";
    return -1;
  }
  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(local_s, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) <
      0) {
    std::cerr << "[server] setsockopt() failed";
    close(local_s);
    return -1;
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);
  if (bind(local_s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "[server] bind() failed";
    close(local_s);
    return -1;
  }

  // Listen back log
  if (listen(local_s, 32) < 0) {
    std::cerr << "[server] listen() failed";
    close(local_s);
    return -1;
  }
  std::cout << "[server] listening on port: " << SERVER_PORT << std::endl;
  std::cout << "[server] waiting for connections ...\n";

  return local_s;
}

void ServerSendWelcomeMsg(int client_fd) {
  std::string s = "welcome! you are client #" + std::to_string(GetConnectionIndex(client_fd)) + "\n";
  int string_length = s.length() + 1;
  char *char_array = new char[string_length];
  strcpy(char_array, s.c_str());
  if (send(client_fd, char_array, string_length, 0) != string_length)
    std::cerr << "[server] send msg error";
  delete[] char_array;
}

void ReceiveMessages(int client_fd) {
  char buff[256];
  memset(buff, 0, sizeof(buff));
  int bytes_read = read(client_fd, buff, sizeof(buff));
  if (bytes_read < 0) {
    std::cerr << "[server] read() failed";
    RemoveConnection(client_fd);
  } else if (bytes_read == 0) {
    std::cout << "[client] #" << GetConnectionIndex(client_fd) << " disconnected.";
    RemoveConnection(client_fd);
  } else
    std::cout << "[client] data #" << GetConnectionIndex(client_fd) << ": " << buff;
}

void EventDemultiplexer(int kq, int local_s) {
  // event want to monitor
  struct kevent evSet;

  // events that were triggered
  struct kevent evList[MAX_EVENTS];

  struct sockaddr_in addr;
  int socklen = sizeof(addr);

  struct timespec tmout = {0,  /* block for 0 seconds at most */
                           0}; /* nanoseconds */

  // Event Loop
  while (1) {
    // returns the number of events placed in the eventlist
    int num_events = kevent(kq, NULL, 0, evList, MAX_EVENTS, &tmout);

    if (num_events < 0) {
      std::cerr << "[server] kevent failed";
      exit(-1);
    } else if (num_events == 0) {
      // std::cout << "Nonblocking!\n";
    } else {
      for (int i = 0; i < num_events; i++) {
        // READ
        if (evList[i].filter == EVFILT_READ) {
          // receive new connection
          if (evList[i].ident == local_s) {
            int fd = accept(evList[i].ident, (struct sockaddr *)&addr,
                            (socklen_t *)&socklen);
            if (fd < 1) {
              std::cerr << "[server] accept failed";
              close(fd);
              exit(-1);
            }

            // Get info client
            getpeername(fd, (struct sockaddr *)&addr,
                        (socklen_t *)&socklen);
            std::cout << "[+] [server] connection accepted from ip: "
                      << inet_ntoa(addr.sin_addr)
                      << " - port: " << ntohs(addr.sin_port)
                      << std::endl;

            // add conection to array
            if (AddConnection(fd) == 0) {
              // event want to monitor
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
            std::cout << "[client] #" << GetConnectionIndex(fd) << " disconnected.\n";

            EV_SET(&evSet, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            kevent(kq, &evSet, 1, NULL, 0, NULL);

            RemoveConnection(fd);
          } else {  // read message from client
            ReceiveMessages(evList[i].ident);
            char msg[80] = "Server send!\n";
            send(evList[i].ident, msg, strlen(msg), 0);
          }
        } else if (evList[i].filter == EVFILT_WRITE) {
          // std::cout << "write!";
        } else {
          // std::cout << "nothing!";
        }
      }
    }
  }
}

void Reactor(const int server_socket) {
  // The kqueue holds all the events we are interested in.
  int kq = kqueue();
  if (kq == -1)
    std::cerr << "kqueue() failed";

  // add sock server to queue monitor
  struct kevent kev {};
  EV_SET(&kev, server_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  kevent(kq, &kev, 1, nullptr, 0, nullptr);

  // The event demultiplexer will push new events to the Event Queue
  EventDemultiplexer(kq, server_socket);
}

int main(int argc, const char *argv[]) {
  int server_fd = CreateSocketAndListen();
  Reactor(server_fd);
  return 0;
}
