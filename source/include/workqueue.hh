#ifndef LIBINV_WORKQUEUE_HH
#define LIBINV_WORKQUEUE_HH
#include <memory>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include "exception.hh"

namespace inventory::RPC {

template<class Work>
class Workqueue {
public:
    typedef std::function<void(Work &)> Handler;

private:
    struct WorkqueueEntry {
        std::unique_ptr<Work> work;
        Handler handler;
    };

public:
    Workqueue(int num_workers)
    : m_stop(false) {
        launch_worker_threads(num_workers);
    }

    ~Workqueue() {
        stop_worker_threads();
    }

    void push(std::unique_ptr<Work> work, Handler handler);
    int size() const {
        std::lock_guard<std::mutex> lock(m_wqmtx);
        return m_wq.size();
    }

private:
    class WorkqueueEmpty {};

    struct WorkqueueEntry pop();
    void launch_worker_threads(unsigned int num);
    void stop_worker_threads();
    void worker_impl();

    mutable std::mutex m_wqmtx;
    mutable std::mutex m_cvmtx;
    mutable std::condition_variable m_wqcv;
    std::deque<struct WorkqueueEntry> m_wq;
    std::vector<std::thread> m_workers;
    bool m_stop;
};

template<class Work>
void Workqueue<Work>::push(std::unique_ptr<Work> request,
                            Workqueue::Handler handler) {
    std::lock_guard<std::mutex> lock(m_wqmtx);
    m_wq.push_back({std::move(request), handler});
    m_wqcv.notify_one();
}

template<class Work>
struct Workqueue<Work>::WorkqueueEntry Workqueue<Work>::pop() {
    std::lock_guard<std::mutex> lock(m_wqmtx);
    if (m_wq.empty())
        throw WorkqueueEmpty();

    struct WorkqueueEntry ret = {
        std::move(m_wq.front().work),
        m_wq.front().handler
    };
    m_wq.pop_front();
    return ret;
}

template<class Work>
void Workqueue<Work>::launch_worker_threads(unsigned int num) {
    for (int i = 0; i < num; ++i)
        m_workers.push_back(std::thread(&Workqueue::worker_impl, this));
}

template<class Work>
void Workqueue<Work>::stop_worker_threads() {
    m_stop = true;
    m_wqcv.notify_all();
    for (std::thread &thread : m_workers)
        thread.join();
}

template<class Work>
void Workqueue<Work>::worker_impl() {
    using namespace std::chrono_literals;

    for (;;) {
        struct WorkqueueEntry entry; 
        {
            if (m_stop)
                break;

            std::unique_lock<std::mutex> lock(m_cvmtx);
            m_wqcv.wait_for(lock, 1s);
            try {
                entry = pop();
            } catch (const WorkqueueEmpty &e) {
                continue;
            }
        }

        entry.handler(*entry.work);
    }
}

}

#endif
