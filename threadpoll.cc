#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>

// Number of worker threads in the thread pool
const int NUM_THREADS = 4;

// Function to be executed by the worker threads
void workerThreadFunc(std::queue<int>& taskQueue, std::mutex& queueMutex, std::condition_variable& cv, std::atomic<bool>& stopFlag) {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);

        // Wait until there's a task in the queue or the stop flag is set
        cv.wait(lock, [&taskQueue, &stopFlag] { return !taskQueue.empty() || stopFlag.load(); });

        // If stop flag is set and the task queue is empty, exit the thread
        if (stopFlag.load() && taskQueue.empty()) {
            break;
        }

        // Retrieve a task from the queue
        int task = taskQueue.front();
        taskQueue.pop();

        // Unlock the mutex before executing the task
        lock.unlock();

        // Process the task
        std::cout << "Processing task: " << task << " on thread: " << std::this_thread::get_id() << std::endl;

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    std::vector<std::thread> workerThreads;
    std::queue<int> taskQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopFlag(false);

    // Create worker threads and add them to the thread pool
    for (int i = 0; i < NUM_THREADS; ++i) {
        workerThreads.emplace_back(workerThreadFunc, std::ref(taskQueue), std::ref(queueMutex), std::ref(cv), std::ref(stopFlag));
    }

    // Add some tasks to the task queue
    for (int i = 0; i < 10; ++i) {
        taskQueue.push(i);
    }

    // Notify worker threads to start processing tasks
    cv.notify_all();

    // Simulate main thread doing some other work
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop worker threads by setting the stop flag
    stopFlag.store(true);

    // Notify worker threads to wake up and exit
    cv.notify_all();

    // Join worker threads with the main thread
    for (std::thread& workerThread : workerThreads) {
        workerThread.join();
    }

    return 0;
}
