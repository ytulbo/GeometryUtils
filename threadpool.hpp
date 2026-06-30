#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>

class ThreadPool {
public:
    // Initialize the thread pool with a specific number of worker threads
    explicit ThreadPool(size_t threads);
    
    // Enqueue a task for execution and return a future tracking its result
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
        
    // Clean up and join all threads gracefully
    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop;
};

// Constructor: Launches the specified amount of worker loops
inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    // Sleep until a task arrives or the pool is stopped
                    this->cv.wait(lock, [this] { 
                        return this->stop || !this->tasks.empty(); 
                    });
                    
                    // Exit thread if pool is stopping and no tasks are left
                    if(this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                // Execute the task outside the locked scope
                task();
            }
        });
    }
}

// Enqueue: Accepts any callable object and its arguments
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    
    using return_type = typename std::invoke_result<F, Args...>::type;

    // Package the task to work with void() queue matching and track futures
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Throw an exception if someone tries to add a task after shutdown
        if(stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks.emplace([task]() { (*task)(); });
    }
    cv.notify_one(); // Wake up one waiting worker thread
    return res;
}

// Destructor: Flags the threads to finish and joins them
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    cv.notify_all(); // Wake up all idle threads to evaluate the stop condition
    for(std::thread &worker: workers) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}
