#include "QueueP2P.h"

#include "stopProgram.h"

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
    const auto iter = queue.insert(queue.end(), std::make_shared<QueueElement>(segment));
    iter->get()->it = iter;
    cond_pop.notify_one();
}
    
bool QueueP2P::getElement(QueueP2PElement &element, const std::function<bool(const Segment &segment, const std::set<std::string> &servers)> &predicate, const std::string &currentServer) {
    std::unique_lock<std::mutex> lock(mut);
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

void QueueP2P::removeElement(const QueueP2PElement &element) {
    std::lock_guard<std::mutex> lock(mut);
    const std::shared_ptr<QueueP2P::QueueElement> lockPtr(element.element.lock());
    if (lockPtr == nullptr) {
        return;
    } else {
        const auto it = lockPtr->it;
        queue.erase(it);
    }
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

void QueueP2P::stop() {
    std::lock_guard<std::mutex> lock(mut);
    isStopped = true;
    cond_pop.notify_all();
}

} // namespace torrent_node_lib 
