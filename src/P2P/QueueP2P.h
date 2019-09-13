#ifndef QUEUE_P2P_H_
#define QUEUE_P2P_H_

#include <set>
#include <list>
#include <memory>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace torrent_node_lib {
    
struct Segment {
    size_t fromByte;
    size_t toByte;
    size_t posInArray;
    
    Segment(size_t fromByte, size_t toByte, size_t posInArray)
        : fromByte(fromByte)
        , toByte(toByte)
        , posInArray(posInArray)
    {}
};
    
class QueueP2PElement;

class QueueP2P {
    friend class QueueP2PElement;
private:
    
    struct QueueElement {
        Segment segment;
        
        std::set<std::string> servers;
        
        std::set<std::string> errorServers;
        
        std::list<std::shared_ptr<QueueElement>>::iterator it;
        
        explicit QueueElement(const Segment &segment)
            : segment(segment)
        {}
    };
    
public:
    
    void addElement(const Segment &segment);
    
    bool getElement(QueueP2PElement &element, const std::function<bool(const Segment &segment, const std::set<std::string> &servers)> &predicate, const std::string &currentServer);
    
    void removeElement(const QueueP2PElement &element);
    
    size_t errorElement(QueueP2PElement &element, const std::string &server);
    
    void stop();
    
private:
    
    mutable std::mutex mut;
    
    std::condition_variable cond_pop;
    
    std::list<std::shared_ptr<QueueElement>> queue;
    
    bool isStopped = false;
    
};

class QueueP2PElement {
    friend class QueueP2P;
public:
    
    QueueP2PElement() = default;
    
private:
        
    QueueP2PElement(std::shared_ptr<QueueP2P::QueueElement> element)
        : element(element)
    {}
    
public:
    
    std::optional<Segment> getSegment() const;
    
private:
    
    std::weak_ptr<QueueP2P::QueueElement> element;
};

} // namespace torrent_node_lib 

#endif // QUEUE_P2P_H_
