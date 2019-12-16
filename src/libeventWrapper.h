#ifndef LIBEVENT_WRAPPER_H_
#define LIBEVENT_WRAPPER_H_

#include <mutex>
#include <string>
#include <atomic>
#include <memory>

#ifdef UBUNTU14
namespace mh {
namespace libevent {
class LibEvent;
}
}
using SyncClient = mh::libevent::LibEvent;
#else
namespace sniper {
namespace http {
class SyncClient;
}
}
using SyncClient = sniper::http::SyncClient;
#endif

struct LibEvent {
public:
    
    struct LibEventInstance {
        friend struct LibEvent;
        
        LibEventInstance();
        
        LibEventInstance(std::unique_ptr<SyncClient> &&libevent);
                
        LibEventInstance(LibEventInstance &&second);
        
        ~LibEventInstance();
        
        LibEventInstance& operator=(LibEventInstance &&second);
                
        std::unique_ptr<SyncClient> libevent;
        
    private:
        
        mutable std::mutex mut;
    };
    
public:
    
    static void initialize();
    
    static void destroy();
    
    static LibEventInstance getInstance();
    
    static std::string request(const LibEventInstance &instance, const std::string &url, const std::string &postData, size_t timeoutSec);
        
private:
    
    static std::mutex initializeMut;
    static std::atomic<bool> isInitialized;
};

#endif // LIBEVENT_WRAPPER_H_
