#include "P2P.h"

#include "check.h"

#include "parallel_for.h"
#include "BlockedQueue.h"
#include "Thread.h"
#include "log.h"

#include "curlWrapper.h"

#include "ReferenceWrapper.h"

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

P2P::P2P(size_t countThreads) {
    for (size_t i = 0; i < countThreads; i++) {
        curls.emplace_back(Curl::getInstance());
        threads.emplace_back(curls.back(), blockedQueue);
    }
}

std::string P2P::request(const CurlInstance &curl, const std::string& qs, const std::string& postData, const std::string& header, const std::string& server) {
    std::string url = server;
    CHECK(!url.empty(), "server empty");
    if (url[url.size() - 1] != '/') {
        url += '/';
    }
    url += qs;
    const std::string response = Curl::request(curl, url, postData, header, "", 5);
    return response;
}

bool P2P::process(const std::vector<ThreadDistribution> &threadsDistribution, const std::vector<Segment> &segments, const MakeQsAndPostFunction &makeQsAndPost, const ProcessResponse &processResponse) {   
    ReferenseWrapperMaster<P2PReferences> referenceWrapper(P2PReferences(makeQsAndPost, processResponse));
    
    const size_t countUniqueServer = countUnique(threadsDistribution, [](const ThreadDistribution &first, const ThreadDistribution &second) {
        return first.server < second.server;
    });
    
    blockedQueue.start(taskId);
    
    for (const ThreadDistribution &distr: threadsDistribution) {
        for (size_t index = distr.from; index < distr.to; index++) {
            CHECK(index < threads.size(), "Incorrect thread number");
            threads[index].newTask(taskId, distr.server, referenceWrapper.makeSlave(), countUniqueServer);
        }
    }
    
    for (const Segment &segment: segments) {
        blockedQueue.addElement(segment);
    }
    
    blockedQueue.waitEmpty();
    const bool isError = blockedQueue.error();
    referenceWrapper.destroy();
    blockedQueue.stop();
    
    checkStopSignal();
    
    taskId++;
    
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
