#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#define SERVER_PORT 9000

int CreateSocketAndListen();

void EventLoop(int kq, int local_s, std::queue<int>& client_queue,
               std::mutex& queue_mutex, std::condition_variable& cv);

// Function to be executed by the worker threads
void workerThreadFunc(std::queue<int>& client_queue, std::mutex& queue_mutex,
                      std::condition_variable& cv);

int main(int argc, const char* argv[]) {
  int server_socket = CreateSocketAndListen();

  // The kqueue holds all the events we are interested in.
  int kq = kqueue();
  if (kq == -1) std::cerr << "kqueue() failed";

  // add sock server to queue monitor
  struct kevent evSet {};
  EV_SET(&evSet, server_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  kevent(kq, &evSet, 1, nullptr, 0, nullptr);

  std::mutex queue_mutex;
  std::condition_variable cv;
  std::queue<int> client_queue;

  // Create worker threads and add them to the thread pool
  int count_threads = std::thread::hardware_concurrency();
  std::vector<std::thread> workerThreads;
  for (int i = 0; i < 4; i++) {
    workerThreads.emplace_back(workerThreadFunc, std::ref(client_queue),
                               std::ref(queue_mutex), std::ref(cv));
  }

  // Event loop
  EventLoop(kq, server_socket, std::ref(client_queue), std::ref(queue_mutex),
            std::ref(cv));

  // Join worker threads with the main thread
  for (auto& thread : workerThreads) {
    thread.join();
  }
  return 0;
}

int CreateSocketAndListen() {
  // create a server socket
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "Socket() failed";
    exit(-1);
  }

  // Allow socket descriptor to be reuseable
  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,
                 sizeof(opt)) < 0) {
    std::cerr << "Setsockopt() failed";
    close(server_socket);
    exit(-1);
  }

  // Bind the socket
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(SERVER_PORT);
  if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    std::cerr << "Bind() failed";
    close(server_socket);
    exit(-1);
  }

  // Listen backlog
  if (listen(server_socket, 32) < 0) {
    std::cerr << "listen() failed";
    close(server_socket);
    exit(-1);
  }
  std::cout << "[Server] Listening on Port " << SERVER_PORT << std::endl;
  std::cout << "Waiting for connections ...\n";

  return server_socket;
}

void EventLoop(int kq, int local_s, std::queue<int>& client_queue,
               std::mutex& queue_mutex, std::condition_variable& cv) {

  [[maybe_unused]] struct timespec tmout = {0, /* block for 0 seconds at most */
                                            0}; /* nanoseconds */
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
      if (events[i].ident == local_s) {
        // Accept new connection
        int client_socket = accept(local_s, nullptr, nullptr);
        if (client_socket < 1) {
          std::cerr << "accept failed";
          close(client_socket);
          exit(-1);
        }

        // Get info client
        getpeername(client_socket, (struct sockaddr*)&client_addr,
                    (socklen_t*)&addrlen);
        std::cout << "[+] Connection accepted from IP : "
                  << inet_ntoa(client_addr.sin_addr)
                  << " and port: " << ntohs(client_addr.sin_port) << std::endl;

        // event want to monitor
        struct kevent kev {};
        EV_SET(&kev, client_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq, &kev, 1, nullptr, 0, nullptr);

        std::lock_guard<std::mutex> lock(queue_mutex);
        client_queue.push(client_socket);

        // Notify worker threads to start processing tasks
        cv.notify_all();
      } else {
        // Handle existing connection
        workerThreadFunc(client_queue, queue_mutex, cv);
      }
    }
  }
}

void workerThreadFunc(std::queue<int>& client_queue, std::mutex& queue_mutex,
                      std::condition_variable& cv) {
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex);

    // Wait until there's a task in the queue or the stop flag is set
    cv.wait(lock, [&client_queue] { return !client_queue.empty(); });

    // If task queue is empty, exit the thread
    if (client_queue.empty()) {
      break;
    }

    // Retrieve a task from the queue
    int client_socket = client_queue.front();
    client_queue.pop();

    // Unlock the mutex before executing the task
    lock.unlock();

    // Process the task
    char buffer[1024];
    struct sockaddr_in address {};
    int addrlen = sizeof(address);

    while (true) {
      memset(buffer, 0, sizeof(buffer));
      int num_bytes = read(client_socket, buffer, sizeof(buffer));
      getpeername(client_socket, (struct sockaddr*)&address,
                  (socklen_t*)&addrlen);
      // Connection closed by client
      if (num_bytes == 0) {
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
        std::cout << "Data [" << inet_ntoa(address.sin_addr) << "-"
                  << ntohs(address.sin_port) << "]: " << buffer << std::endl;

        // Client send 'exit'
        if ((strncmp(buffer, "exit", 4)) == 0) {
          std::cout << "Client disconnected ip " << inet_ntoa(address.sin_addr)
                    << " port " << ntohs(address.sin_port) << std::endl;
          close(client_socket);
          break;
        }
      }
    }
  }
}
