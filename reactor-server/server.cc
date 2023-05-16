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

const int NUM_THREADS = 4;

// Function to be executed by the worker threads
void workerThreadFunc(std::queue<int>& client_queue, std::mutex& queue_mutex,
                      std::condition_variable& cv) {
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex);

    // Wait until there's a task in the queue or the stop flag is set
    cv.wait(lock, [&client_queue] { return !client_queue.empty(); });

    // If stop flag is set and the task queue is empty, exit the thread
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
    while (true) {
      int num_bytes = recv(client_socket, buffer, sizeof(buffer), 0);
      if (num_bytes == 0) {
        // Connection closed by client
        std::cout << "Client disconnected\n";
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
        std::cout << "Received data: " << buffer << std::endl;
      }
    }
  }
}

int CreateSocketAndListen() {
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

  // Listen back log
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
  while (true) {
    struct kevent events[10];
    int num_events = kevent(kq, nullptr, 0, events, 10, nullptr);
    if (num_events < 0) {
      std::cerr << "Error polling for events\n";
      exit(-1);
    }

    for (int i = 0; i < num_events; i++) {
      if (events[i].ident == local_s) {
        // Accept new connection
        int client_socket = accept(local_s, nullptr, nullptr);
        struct kevent kev {};
        EV_SET(&kev, client_socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq, &kev, 1, nullptr, 0, nullptr);

        std::lock_guard<std::mutex> lock(queue_mutex);
        client_queue.push(client_socket);
        cv.notify_one();
      } else {
        // Handle existing connection
        workerThreadFunc(client_queue, queue_mutex, cv);
      }
    }
  }
}

int main() {
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
  std::vector<std::thread> workerThreads;
  for (int i = 0; i < NUM_THREADS; i++) {
    workerThreads.emplace_back(workerThreadFunc, std::ref(client_queue),
                               std::ref(queue_mutex), std::ref(cv));
  }

  // event loop
  EventLoop(kq, server_socket, std::ref(client_queue), std::ref(queue_mutex),
            std::ref(cv));

  // Join worker threads with the main thread
  for (auto& thread : workerThreads) {
    thread.join();
  }
  return 0;
}
