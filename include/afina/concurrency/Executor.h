#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(std::string name, int size, std::function<void(const std::string &msg)> &log_err_, size_t min_threads = 2,
             size_t max_threads = 4, size_t idle_time = 3000)
        : _name(std::move(name)), max_queue_size(size), low_watermark(min_threads), high_watermark(max_threads),
          idle_time(idle_time) {
        log_err = log_err_;
        std::unique_lock<std::mutex> lock(this->mutex);
        for (int i = 0; i < low_watermark; ++i) {
            threads.emplace_back(std::thread([this] { return perform(this); }));
        }
    };

    ~Executor() {
        if (state != State::kStopped) {
            Stop(true);
        }
    };

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        state = State::kStopping;
        std::unique_lock<std::mutex> lock(this->mutex);
        empty_condition.notify_all();
        tasks.clear();
        if (await) {
            no_more_threads.wait(lock, [&, this] { return this->threads.empty(); });
        }
    };

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        if (state != State::kRun || cur_queue_size >= max_queue_size) {
            return false;
        }

        // Enqueue new task
        std::unique_lock<std::mutex> lock(this->mutex);
        if (cur_queue_size++ > 0 && threads.size() < high_watermark) {
            threads.emplace_back(std::thread([this] { return perform(this); }));
        }
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    };

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    std::function<void(const std::string &msg)> log_err;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor) {
        std::function<void()> task;
        auto finish_thread = [executor] {
            auto this_thread = std::this_thread::get_id();
            for (auto it = executor->threads.begin(); it < executor->threads.end(); ++it) {
                if (it->get_id() == this_thread) {
                    auto self = std::move(*it);
                    self.detach();
                    executor->threads.erase(it);
                    if (executor->threads.empty()) {
                        executor->state = State::kStopped;
                        executor->no_more_threads.notify_all();
                    }
                    return;
                }
            }
        };

        while (executor->state == Executor::State::kRun) {
            {
                std::unique_lock<std::mutex> lock(executor->mutex);
                while (executor->tasks.empty()) {
                    executor->empty_condition.wait_for(lock, std::chrono::milliseconds(executor->idle_time));
                    if ((executor->tasks.empty() && executor->threads.size() > executor->low_watermark) ||
                        executor->state != Executor::State::kRun) {
                        finish_thread();
                        return;
                    }
                }
                task = std::move(executor->tasks.front());
                executor->tasks.pop_front();
                executor->cur_queue_size--;
            }
            try {
                task();
            } catch (const std::exception &ex) {
                executor->log_err(ex.what());
            } catch (const std::string &ex) {
                executor->log_err(ex);
            } catch (...) {
                executor->log_err("Unknown exception");
            }
        }
        finish_thread();
    };

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    std::atomic<State> state{State::kRun};

    /**
     * Minimum threads in pool
     */
    size_t low_watermark;

    /**
     * Maximum threads in pool
     */
    size_t high_watermark;

    /**
     * Maximum number of tasks in queue
     */
    size_t max_queue_size;
    size_t cur_queue_size = 0;
    /**
     * Maximum milliseconds to wait before thread stopped if count of thread above low_watermark
     */
    size_t idle_time;

    /**
     * Maximum milliseconds to wait before thread stopped if count of thread above low_watermark
     */
    std::string _name;

    /**
     * Conditional variable to await executor stop
     */
    std::condition_variable no_more_threads;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
