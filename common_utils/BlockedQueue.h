#ifndef BLOCKED_QUEUE_H_
#define BLOCKED_QUEUE_H_

#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>

#include "stopProgram.h"

namespace common {

template <typename T, size_t MAX_SIZE = 1>
class BlockedQueue {
private:
    
    mutable std::mutex d_mutex;
    std::condition_variable cond_pop;
    std::condition_variable cond_push;
    std::deque<T> d_queue;
    bool isStopped = false;
    
public:
    
    void push(const T& value) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_push, lock, [this]{ return d_queue.size() < MAX_SIZE || isStopped; });
        if (isStopped) {
            return;
        }
        d_queue.push_front(value);
        cond_pop.notify_one();
    }
    
    void push(T&& value) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_push, lock, [this]{ return d_queue.size() < MAX_SIZE || isStopped; });
        if (isStopped) {
            return;
        }
        d_queue.push_front(std::forward<T>(value));
        cond_pop.notify_one();
    }
    
    bool pop(T &t) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_pop, lock, [this]{ return !d_queue.empty() || isStopped; });
        if (isStopped) {
            return false;
        }
        t = std::move(d_queue.back());
        d_queue.pop_back();
        cond_push.notify_one();
        return true;
    }
    
    template<typename F>
    bool pop(T &t, const F &predicate) {
        typename std::deque<T>::reverse_iterator it;
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_pop, lock, [this, &it, &predicate]{ 
            it = std::find_if(d_queue.rbegin(), d_queue.rend(), predicate);
            return it != d_queue.rend() || isStopped; 
        });
        if (isStopped) {
            return false;
        }
        
        t = std::move(*it);
        d_queue.erase(std::next(it).base());
        cond_push.notify_one();
        return true;
    }
    
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(d_mutex);
        return d_queue.empty();
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(d_mutex);
        isStopped = true;
        cond_pop.notify_all();
        cond_push.notify_all();
    }
    
};

}

#endif // BLOCKED_QUEUE_H_
