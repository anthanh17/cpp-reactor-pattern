
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

enum class EventType {
  kNone,
  kAccept,
  kRead,
  kWrite,
  kReadWrite,
  kClientDisconnection,
  kCount,
};

int GetConnectionIndex(const int fd) {
  for (int i = 0; i < NUM_CLIENTS; i++)
    if (clients_fd[i] == fd) return i;
  return -1;
}

int AddConnection(const int fd) {
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

int RemoveConnection(const int fd) {
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
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "[server] socket() failed";
    return -1;
  }
  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) <
      0) {
    std::cerr << "[server] setsockopt() failed";
    close(server_fd);
    return -1;
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);
  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "[server] bind() failed";
    close(server_fd);
    return -1;
  }

  // Listen back log
  if (listen(server_fd, 32) < 0) {
    std::cerr << "[server] listen() failed";
    close(server_fd);
    return -1;
  }
  std::cout << "[server] listening on port: " << SERVER_PORT << std::endl;
  std::cout << "[server] waiting for connections ...\n";

  return server_fd;
}

void ServerSendWelcomeMsg(const int client_fd) {
  std::string s = "welcome! you are client #" + std::to_string(GetConnectionIndex(client_fd)) + "\n";
  int string_length = s.length() + 1;
  char *char_array = new char[string_length];
  strcpy(char_array, s.c_str());
  if (send(client_fd, char_array, string_length, 0) != string_length)
    std::cerr << "[server] send msg error";
  delete[] char_array;
}

void ReceiveMessages(const int client_fd) {
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

void ServerAcceptConnection(const int kq, const int server_fd) {
  struct sockaddr_in addr;
  int socklen = sizeof(addr);

  int client_fd = accept(server_fd, (struct sockaddr *)&addr,
                         (socklen_t *)&socklen);
  if (client_fd < 1) {
    std::cerr << "[server] accept failed";
    close(client_fd);
    exit(-1);
  }

  // Get info client
  getpeername(client_fd, (struct sockaddr *)&addr,
              (socklen_t *)&socklen);
  std::cout << "[+] [server] connection accepted from ip: "
            << inet_ntoa(addr.sin_addr)
            << " - port: " << ntohs(addr.sin_port)
            << std::endl;

  // add connection to array
  if (AddConnection(client_fd) == 0) {
    // event want to monitor
    struct kevent evSet;
    EV_SET(&evSet, client_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &evSet, 1, NULL, 0, NULL);

    ServerSendWelcomeMsg(client_fd);
  } else {
    printf("Add failed connection.\n");
    close(client_fd);
  }
}

void ClientDisconnection(const int kq, const int client_fd) {
  std::cout << "[client] #" << GetConnectionIndex(client_fd) << " disconnected.\n";

  // event want to monitor
  struct kevent evSet;
  EV_SET(&evSet, client_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  kevent(kq, &evSet, 1, NULL, 0, NULL);

  RemoveConnection(client_fd);
}

void EventHandlers(EventType event, const int fd, const int kq) {
  switch (event) {
    case EventType::kAccept:
      ServerAcceptConnection(kq, fd);
      break;
    case EventType::kRead:
      ReceiveMessages(fd);
      break;
    case EventType::kWrite:
      ServerSendWelcomeMsg(fd);
      break;
    case EventType::kReadWrite:
      break;
    case EventType::kClientDisconnection:
      ClientDisconnection(kq, fd);
      break;

    default:
      break;
  }
}

void EventDemultiplexer(const int kq, const int server_fd) {
  // events that were triggered
  struct kevent evList[MAX_EVENTS];

  struct timespec time_out = {0,  /* block for 0 seconds at most */
                              0}; /* nanoseconds */

  while (1) {
    // returns the number of events placed in the eventlist
    int num_events = kevent(kq, NULL, 0, evList, MAX_EVENTS, &time_out);

    if (num_events < 0) {
      std::cerr << "[server] kevent failed";
      exit(-1);
    } else if (num_events == 0) {
      // std::cout << "Nonblocking!\n";
    } else {
      // Event Loop
      for (int i = 0; i < num_events; i++) {
        // READ
        if (evList[i].filter == EVFILT_READ) {
          // Server - receive new connection
          if (evList[i].ident == server_fd) {
            EventHandlers(EventType::kAccept, server_fd, kq);
          }  // client disconnected
          else if (evList[i].flags & EV_EOF) {
            EventHandlers(EventType::kClientDisconnection, evList[i].ident, kq);

          } else {  // read message from client()
            EventHandlers(EventType::kRead, evList[i].ident, kq);
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

void Reactor(const int server_fd) {
  // The kqueue holds all the events we are interested in.
  int kq = kqueue();
  if (kq == -1)
    std::cerr << "kqueue() failed";

  // add sock server to queue monitor
  struct kevent kev {};
  EV_SET(&kev, server_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  kevent(kq, &kev, 1, nullptr, 0, nullptr);

  // The event demultiplexer will push new events to the Event Queue
  EventDemultiplexer(kq, server_fd);
}

int main(int argc, const char *argv[]) {
  int server_fd = CreateSocketAndListen();
  Reactor(server_fd);
  return 0;
}
