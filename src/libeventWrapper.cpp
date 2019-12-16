#include "libeventWrapper.h"

#ifdef UBUNTU14
#include <mh/libevent/LibEvent.h>
#else
#include <sniper/http/SyncClient.h>
#endif

#include "check.h"
#include "OopUtils.h"
#include "duration.h"

#include "log.h"

#include <memory>
#include <algorithm>

std::mutex LibEvent::initializeMut;
std::atomic<bool> LibEvent::isInitialized(false);


void LibEvent::initialize() {
    std::lock_guard<std::mutex> lock(initializeMut);
    if (isInitialized.load()) {
        return;
    }

    isInitialized = true;
}

void LibEvent::destroy() {
    //c не удаляем, чтобы один поток не почистил ресурсы, пока им пользуется другой поток
    // curl_global_cleanup();
}

LibEvent::LibEventInstance LibEvent::getInstance() {
    CHECK(isInitialized.load(), "not initialized");
    
#ifdef UBUNTU14
    std::unique_ptr<SyncClient> libevent = std::make_unique<SyncClient>();
#else
    std::unique_ptr<SyncClient> libevent = std::make_unique<SyncClient>(10s);
#endif
    CHECK(libevent != nullptr, "libevent == nullptr");
    
    return LibEventInstance(std::move(libevent));
}

std::string LibEvent::request(const LibEvent::LibEventInstance& instance, const std::string& url, const std::string& postData, size_t timeoutSec) {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_lock<std::mutex> lock(instance.mut, std::try_to_lock);
    CHECK(lock.owns_lock(), "Curl instanse one of thread");
    
    CHECK(instance.libevent != nullptr, "Incorrect curl instance");
    SyncClient &libevent = *instance.libevent.get();
    
#ifdef UBUNTU14
    const size_t found = url.find_last_of(":");
    std::string host = url;
    int port = 80;
    if (found != url.npos && url.substr(0, found) != "http" && url.substr(0, found) != "https") {
        host = url.substr(0, found);
        port = std::stoi(url.substr(found + 1));
    }
    std::string path;
    const size_t foundPath = url.find("/");
    if (foundPath != url.npos) {
        host = url.substr(0, std::min(host.size(), foundPath));
        path = url.substr(foundPath);
    }
    
    std::string response;
    libevent.post_keep_alive(host, port, host, path, postData, response, timeoutSec * 1000);
    
    return response;
#else
    const auto response = libevent.post(url, postData);
    return std::string(response->data());
#endif
}

LibEvent::LibEventInstance::LibEventInstance()
    : libevent(nullptr)
{}

LibEvent::LibEventInstance::LibEventInstance(std::unique_ptr<SyncClient> &&libevent)
    : libevent(std::move(libevent))
{}

LibEvent::LibEventInstance::LibEventInstance(LibEvent::LibEventInstance &&second)
    : libevent(std::move(second.libevent))
{}

LibEvent::LibEventInstance &LibEvent::LibEventInstance::operator=(LibEvent::LibEventInstance &&second) {
    this->libevent = std::move(second.libevent);
    return *this;
}

LibEvent::LibEventInstance::~LibEventInstance() = default;

