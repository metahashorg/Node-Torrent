#include "QueueP2P.h"

#include "stopProgram.h"
#include "check.h"

using namespace common;

namespace torrent_node_lib {
    
std::optional<Segment> QueueP2PElement::getSegment() const {
    std::shared_ptr<QueueP2P::QueueElement> lock(element.lock());
    if (lock == nullptr) {
        return std::nullopt;
    } else {
        return lock->segment;
    }
}
    
void QueueP2P::addElement(const Segment &segment) {
    std::lock_guard<std::mutex> lock(mut);
    CHECK(!isStopped, "Already stopped");
    const auto iter = queue.insert(queue.end(), std::make_shared<QueueElement>(segment));
    iter->get()->it = iter;
    cond_pop.notify_one();
    cond_empty.notify_all();
}
    
bool QueueP2P::getElement(QueueP2PElement &element, const std::function<bool(const Segment &segment, const std::set<std::string> &servers)> &predicate, const std::string &currentServer, size_t taskId) {
    std::unique_lock<std::mutex> lock(mut);
    CHECK(taskId <= this->taskId, "Ups");
    if (taskId != this->taskId) {
        return false;
    }
    typename std::list<std::shared_ptr<QueueElement>>::reverse_iterator it;
    conditionWait(cond_pop, lock, [this, &it, &predicate]{ 
        if (isStopped) {
            return true;
        }
        const auto p = [&predicate](const std::shared_ptr<QueueElement> &ptr) {
            return predicate(ptr->segment, ptr->servers);
        };
        it = std::find_if(queue.rbegin(), queue.rend(), p);
        return it != queue.rend(); 
    });
    if (isStopped) {
        return false;
    }
    
    typename std::list<std::shared_ptr<QueueElement>>::iterator normIt = (std::next(it)).base();
    normIt->get()->servers.insert(currentServer);
    element = QueueP2PElement(*normIt);
    queue.splice(queue.begin(), queue, normIt);
    return true;
}

void QueueP2P::removeElementInternal(const QueueP2PElement &element) {
    const std::shared_ptr<QueueP2P::QueueElement> lockPtr(element.element.lock());
    if (lockPtr == nullptr) {
        return;
    } else {
        const auto it = lockPtr->it;
        queue.erase(it);
        cond_empty.notify_all();
    }
}

void QueueP2P::removeElement(const QueueP2PElement &element) {
    std::lock_guard<std::mutex> lock(mut);
    removeElementInternal(element);
}

void QueueP2P::removeElementError(const QueueP2PElement &element) {
    std::lock_guard<std::mutex> lock(mut);
    removeElement(element);
    isError = true;
}

size_t QueueP2P::errorElement(QueueP2PElement &element, const std::string &server) {
    std::lock_guard<std::mutex> lock(mut);
    const std::shared_ptr<QueueP2P::QueueElement> lockPtr(element.element.lock());
    if (lockPtr == nullptr) {
        return 0;
    } else {
        lockPtr->errorServers.insert(server);
        return lockPtr->errorServers.size();
    }
}

void QueueP2P::waitEmpty() const {
    std::unique_lock<std::mutex> lock(mut);
    conditionWait(cond_empty, lock, [this]{
        if (isStopped) {
            return true;
        }
        return queue.empty();
    });
}

void QueueP2P::stop() {
    std::lock_guard<std::mutex> lock(mut);
    isStopped = true;
    isError = false;
    cond_pop.notify_all();
    cond_empty.notify_all();
}

void QueueP2P::start(size_t taskId) {
    std::lock_guard<std::mutex> lock(mut);
    CHECK(isStopped, "Already started");
    isStopped = false;
    isError = false;
    CHECK(taskId > this->taskId, "Incorrect sequences task");
    this->taskId = taskId;
    cond_pop.notify_all();
    cond_empty.notify_all();
}

std::optional<size_t> QueueP2P::started() const {
    std::lock_guard<std::mutex> lock(mut);
    if (isStopped) {
        return std::nullopt;
    } else {
        return taskId;
    }
}

bool QueueP2P::error() const {
    std::lock_guard<std::mutex> lock(mut);
    CHECK(!isStopped, "Already stopped");
    return isError;
}

} // namespace torrent_node_lib 
