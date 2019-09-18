#include "P2P.h"

#include "check.h"

#include "parallel_for.h"
#include "BlockedQueue.h"
#include "Thread.h"
#include "log.h"

#include "curlWrapper.h"

using namespace common;

namespace torrent_node_lib {

std::vector<Segment> P2P::makeSegments(size_t countSegments, size_t size, size_t minSize) {
    const size_t step = std::min(size, std::max(size / countSegments, minSize));
    CHECK(step != 0, "step == 0");
    
    std::vector<Segment> answer;
    answer.reserve(countSegments);
    size_t prevByte = 0;
    for (size_t i = 0; i < countSegments - 1; i++) {
        const size_t nextByte = std::min(prevByte + step, size);
        answer.emplace_back(prevByte, nextByte, i);
        prevByte = nextByte;
    }
    answer.emplace_back(prevByte, size, countSegments - 1);
    
    return answer;
}

template<typename T, typename F>
static size_t countUnique(const std::vector<T> &elements, const F &compare) {
    std::vector<T> copy = elements;
    std::sort(copy.begin(), copy.end(), compare);
    const auto endIter = std::unique(copy.begin(), copy.end(), [&compare](const T &first, const T &second) {
        return !(compare(first, second) ^ compare(second, first));
    });
    return std::distance(copy.begin(), endIter);
}

bool P2P::process(const std::vector<std::pair<std::reference_wrapper<const Server>, std::reference_wrapper<const common::CurlInstance>>> &requestServers, const std::vector<Segment> &segments, const MakeQsAndPostFunction &makeQsAndPost, const RequestFunction &requestFunction) {   
    QueueP2P blockedQueue;
    
    const size_t countUniqueServer = countUnique(requestServers, [](const auto &first, const auto &second) {
        return first.first.get().server < second.first.get().server;
    });
    
    std::vector<Thread> threads;
    threads.reserve(requestServers.size());
    for (const auto &[server, curl]: requestServers) {
        threads.emplace_back([&blockedQueue, &requestFunction, &makeQsAndPost, countUniqueServer](const Server &server, const common::CurlInstance &curl){
            try {
                while (true) {
                    QueueP2PElement element;
                    const bool isStopped = !blockedQueue.getElement(element, [&server](const Segment &segment, const std::set<std::string> &servers) {
                        return servers.find(server.server) == servers.end();
                    }, server.server);
                    if (isStopped) {
                        break;
                    }
                    
                    const std::optional<Segment> segment = element.getSegment();
                    if (!segment.has_value()) {
                        continue;
                    }
                    
                    const auto &[qs, post] = makeQsAndPost(segment->fromByte, segment->toByte);
                    try {
                        requestFunction(qs, post, server.server, curl, *segment);
                        blockedQueue.removeElement(element);
                    } catch (const exception &e) {
                        LOGWARN << "Error " << e << " " << server.server;
                        const size_t countErrors = blockedQueue.errorElement(element, server.server);
                        if (countErrors >= countUniqueServer) {
                            blockedQueue.removeElement(element);
                        }
                        break;
                    }
                    
                    checkStopSignal();
                }
            } catch (const exception &e) {
                LOGERR << e;
            } catch (const StopException &e) {
                LOGINFO << "Stop p2p::request thread";
            } catch (const std::exception &e) {
                LOGERR << e.what();
            } catch (...) {
                LOGERR << "Unknown error";
            }
        },
        server, std::cref(curl)
        );
    }
    
    for (const Segment &segment: segments) {
        blockedQueue.addElement(segment);
    }
    
    blockedQueue.waitEmpty();
    const bool isError = blockedQueue.error();
    blockedQueue.stop();
    for (auto &thread: threads) {
        thread.join();
    }
    
    checkStopSignal();
    
    return !isError;
}

SendAllResult P2P::process(const std::vector<std::reference_wrapper<const Server>> &requestServers, const std::string &qs, const std::string &post, const std::string &header, const RequestFunctionSimple &requestFunction) {
    SendAllResult result;
    std::mutex mutResult;
    
    parallelFor(8, requestServers.begin(), requestServers.end(), [&result, &mutResult, &qs, &post, &header, &requestFunction](const Server &server) {
        Timer tt;
        ResponseParse r;
        try {
            r.response = requestFunction(qs, post, header, server.server);
        } catch (const exception &e) {
            r.error = e;
        }
        tt.stop();
        
        checkStopSignal();
        
        std::lock_guard<std::mutex> lock(mutResult);
        result.results.emplace_back(server.server, r, tt.count());
    });
    
    return result;
}

}
