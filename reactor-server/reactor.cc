#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#define SERVER_PORT 9000

const int MAX_CONNECTIONS = 10;
const int NUM_THREADS = 4;

std::mutex queue_mutex;
std::condition_variable condition;
std::queue<int> client_queue;
std::atomic<bool> stop_flag(false);

int CreateSocketAndListen() {
  // create a server socket
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "Socket() failed";
    return -1;
  }

  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt)) < 0) {
    std::cerr << "Setsockopt() failed";
    close(server_socket);
    return -1;
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);
  if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Bind() failed";
    close(server_socket);
    return -1;
  }

  // Listen backlog
  if (listen(server_socket, 32) < 0) {
    std::cerr << "listen() failed";
    close(server_socket);
    return -1;
  }
  std::cout << "[Server] Listening on Port " << SERVER_PORT << std::endl;
  std::cout << "Waiting for connections ...\n";

  return server_socket;
}

void Handler(const int client_socket) {
  char buffer[1024];
  struct sockaddr_in address {};
  int addrlen = sizeof(address);

  getpeername(client_socket, (struct sockaddr *)&address,
              (socklen_t *)&addrlen);

  while (true) {
    int num_bytes = recv(client_socket, buffer, sizeof(buffer), 0);
    if (num_bytes == 0) {
      // Connection closed by client
      std::cout << "Client disconnected ip " << inet_ntoa(address.sin_addr)
                << " port " << ntohs(address.sin_port) << std::endl;
      close(client_socket);
      break;
    } else if (num_bytes < 0) {
      // Error occurred
      std::cerr << "Error reading from client\n";
      close(client_socket);
      break;
    } else {
      // Process data
      buffer[num_bytes] = '\0';
      std::cout << "Data [" << inet_ntoa(address.sin_addr) << "-"
                << ntohs(address.sin_port) << "]: " << buffer;
    }
  }
}

void EventLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex);

    // Wait until there's a task in the queue or the stop flag is set
    condition.wait(lock, [] { return !client_queue.empty() || stop_flag.load(); });

    // If stop flag is set and the task queue is empty, exit the thread
    if (stop_flag.load() && client_queue.empty()) {
      break;
    }

    // Retrieve a task from the queue
    int client_socket = client_queue.front();
    client_queue.pop();

    // Unlock the mutex before executing the task
    lock.unlock();

    // Process the task
    Handler(client_socket);
  }
}

void EventDemultiplexer(const int kq, const int server_socket) {
  // struct timespec tmout = {0, /* block for 0 seconds at most */
  //                                         0}; /* nanoseconds */
  struct sockaddr_in client_addr;
  int addrlen = sizeof(client_addr);

  while (true) {
    // events that were triggered
    struct kevent events[10];

    // returns the number of events placed in the eventlist
    int num_events = kevent(kq, nullptr, 0, events, 10, nullptr);

    if (num_events < 0) {
      std::cerr << "Error polling for events\n";
      exit(-1);
    }
    for (int i = 0; i < num_events; i++) {
      if (events[i].ident == server_socket) {
        // Accept new connection
        int client_socket = accept(server_socket, nullptr, nullptr);

        // Get info client
        getpeername(client_socket, (struct sockaddr *)&client_addr,
                    (socklen_t *)&addrlen);
        std::cout << "[+] Connection accepted from IP : "
                  << inet_ntoa(client_addr.sin_addr)
                  << " and port: " << ntohs(client_addr.sin_port)
                  << std::endl;

        // event want to monitor
        struct kevent kev {};
        EV_SET(&kev, client_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq, &kev, 1, nullptr, 0, nullptr);

        std::lock_guard<std::mutex> lock(queue_mutex);
        client_queue.push(client_socket);

        // Notify worker threads to start processing tasks
        condition.notify_one();
      } else {
        // Handle existing connection
        EventLoop();
      }
    }
  }
}

int main() {
  int server_socket = CreateSocketAndListen();

  // The kqueue holds all the events we are interested in.
  int kq = kqueue();

  // add sock server to queue monitor
  struct kevent kev {
  };
  EV_SET(&kev, server_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  kevent(kq, &kev, 1, nullptr, 0, nullptr);

  // Create worker threads and add them to the thread pool
  std::vector<std::thread> worker_threads;
  for (int i = 0; i < NUM_THREADS; i++) {
    worker_threads.emplace_back(EventLoop);
  }

  // The event demultiplexer will push new events to the Event Queue
  EventDemultiplexer(kq, server_socket);

  // Stop worker threads by setting the stop flag
  stop_flag.store(true);

  // notify all the workers to stop
  condition.notify_all();

  // Join worker threads with the main thread
  for (auto &worker : worker_threads) {
    if (worker.joinable())
      worker.join();
  }

  return 0;
}
