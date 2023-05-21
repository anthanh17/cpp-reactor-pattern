#include <arpa/inet.h>  // inet
#include <netdb.h>      // NI_MAXSERV, NI_MAXHOST
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
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
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == 0) {
    std::cerr << ("[Server] Socket failed!");
    exit(EXIT_FAILURE);
  }
  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) <
      0) {
    std::cerr << "[Server] setsockopt() failed";
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  // Bind
  struct sockaddr_in serverAddr;
  memset(&serverAddr, '\0', sizeof(serverAddr));
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);

  if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "[Server] Bind failed!";
    exit(EXIT_FAILURE);
  }

  // Listen
  if (listen(sockfd, 3) < 0) {
    std::cerr << "[Server] listen failed!";
    exit(EXIT_FAILURE);
  }
  std::cout << "[Server] Listening on Port " << PORT << std::endl;

  // epoll init
  int efd;
  if ((efd = epoll_create1(0)) == -1) {
    std::cerr << "[Server] epoll_create1 failed!";
    exit(EXIT_FAILURE);
  }

  struct epoll_event ev, ep_event[11];
  ev.events = EPOLLIN;
  ev.data.fd = sockfd;

  if (epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
    std::cerr << "[Server] epoll_ctl failed!";
    exit(EXIT_FAILURE);
  }

  int nfds = 0;

  double time_spent = 0.0;
  clock_t begin = clock();

  struct sockaddr_in clienAddr;

  socklen_t addr_size;

  char mssg[100];

  while (1) {

    if ((nfds = epoll_wait(efd, ep_event, 11, -1)) < 0)
      std::cerr << "[Server] error at epoll!";

    for (int i = 0; i < nfds; i++) {
      // check if this fd is ready for reading
      if ((ep_event[i].events & EPOLLIN) == EPOLLIN) {

        if (ep_event[i].data.fd == sockfd) {  // request for new connection

          int newSocket =
              accept(sockfd, (struct sockaddr *)&clienAddr, &addr_size);
          if (newSocket < 0) {
            exit(1);
          }

          ev.events = EPOLLIN;
          ev.data.fd = newSocket;
          if (epoll_ctl(efd, EPOLL_CTL_ADD, newSocket, &ev) == -1)
            std::cerr << "[Server] error at epollctl!";

          char *IP = inet_ntoa(clienAddr.sin_addr);
          int PORT_NO = ntohs(clienAddr.sin_port);

          printf("Connection accepted from IP : %s: and PORT : %d\n", IP,
                 PORT_NO);

        } else {  // some client is sending data

          bzero(mssg, sizeof(mssg));
          int numbytes = read(ep_event[i].data.fd, mssg, sizeof(mssg));
          if (numbytes < 0) {
            std::cerr << "read() failed";
            continue;
          } else if (numbytes == 0) {
            std::cout << "Connection closed\n";
            continue
          }
          std::cout << "Data received: " << mssg;
          //   send(ep_event[i].data.fd, &mssg, sizeof(mssg), 0);
        }
      }
    }
  }

  clock_t end = clock();
  time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
  std::cout << "The elapsed time is " << time_spent << "seconds\n";

  return 0;
}
