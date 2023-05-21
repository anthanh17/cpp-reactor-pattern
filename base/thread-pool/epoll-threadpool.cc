#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

const int MAX_CONNECTIONS = 10;
const int BUFFER_SIZE = 1024;
const int NUM_THREADS = 4;

std::mutex queue_mutex;
std::condition_variable cv;
std::queue<int> client_queue;

void thread_func() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, []{ return !client_queue.empty(); });
        int client_socket = client_queue.front();
        client_queue.pop();
        lock.unlock();

        char buffer[BUFFER_SIZE];
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

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8888);
    server_address.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address));

    listen(server_socket, MAX_CONNECTIONS);

    int epoll_fd = epoll_create1(0);
    struct epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (true) {
        struct epoll_event events[10];
        int num_events = epoll_wait(epoll_fd, events, 10, -1);
        if (num_events < 0) {
            std::cerr << "Error polling for events\n";
            return -1;
        }
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_socket) {
                // Accept new connection
                int client_socket = accept(server_socket, nullptr, nullptr);
                struct epoll_event event{};
                event.events = EPOLLIN;
                event.data.fd = client_socket;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event);

                std::lock_guard<std::mutex> lock(queue_mutex);
                client_queue.push(client_socket);
                cv.notify_one();
            } else {
                // Handle existing connection
                thread_func();
            }
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
