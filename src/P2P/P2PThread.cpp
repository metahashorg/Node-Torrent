#include "P2PThread.h"

#include "curlWrapper.h"

#include "stopProgram.h"
#include "check.h"
#include "log.h"
#include "duration.h"

#include "QueueP2P.h"
#include "P2P.h"

using namespace common;

namespace torrent_node_lib {
    
P2PThread::P2PThread(QueueP2P &queue) 
    : curl(std::make_unique<CurlInstance>(Curl::getInstance()))
    , queue(queue)
{}

P2PThread::~P2PThread() {
    std::unique_lock<std::mutex> lock(mut);
    stopped = true;
    cond.notify_one();
    lock.unlock();
    if (th.joinable()) {
        th.join();
    }
}

void P2PThread::work() {
    try {
        while (true) {
            try {
                std::unique_lock<std::mutex> lock(mut);
                const size_t currId = taskNumber;
                const std::string server = currentServer;
                const ReferenseWrapperSlave<P2PReferences> referenceWrapper = *this->referencesWrapper;
                const size_t countUniqueSv = this->countUniqueServers;
                lock.unlock();
                
                QueueP2PElement element;
                const bool isClosed = !queue.getElement(element, [&server](const Segment &segment, const std::set<std::string> &servers) {
                    return servers.find(server) == servers.end();
                }, server, currId);
                if (isClosed) {
                    std::unique_lock<std::mutex> lock2(mut);
                    Timer timerWait;
                    bool isTimeout = true;
                    while (isTimeout) {
                        isTimeout = !conditionWaitTimeout(cond, lock2, [currId, this]{
                            const auto taskNumberChanged = [currId, this]{
                                return currId != this->taskNumber;
                            };
                            return taskNumberChanged() || stopped;
                        });
                        if (isTimeout && timerWait.count() >= 30s) {
                            curl = nullptr;
                        }
                    }
                    if (stopped) {
                        break;
                    } else {
                        continue;
                    }
                }
                
                const std::optional<Segment> segment = element.getSegment();
                if (!segment.has_value()) {
                    continue;
                }
                
                const auto &[qs, post] = referenceWrapper->get()->makeQsAndPost(segment->fromByte, segment->toByte); // TODO понять как сделать красивую стрелочку
                try {
                    if (curl == nullptr) {
                        curl = std::make_unique<CurlInstance>(Curl::getInstance());
                    }
                    const std::string response = P2P::request(*curl, qs, post, "", server);
                    referenceWrapper->get()->processResponse(response, *segment);
                    queue.removeElement(element);
                } catch (const exception &e) {
                    LOGWARN << "Error " << e << " " << server;
                    const size_t countErrors = queue.errorElement(element, server);
                    if (countErrors >= countUniqueSv) {
                        queue.removeElementError(element);
                    }
                }
            } catch (const DestroyedException&) {
                // empty
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
}

void P2PThread::newTask(size_t taskNum, const std::string &server, const ReferenseWrapperSlave<P2PReferences> &references, size_t countUniqueServers) {
    const auto queueStarted = queue.started();
    CHECK(queueStarted.has_value() && queueStarted.value() == taskNum, "Queue not started");
    
    std::unique_lock<std::mutex> lock(mut);
    CHECK(taskNum > taskNumber, "Incorrect sequences task");
    taskNumber = taskNum;
    currentServer = server;
    referencesWrapper = std::make_unique<ReferenseWrapperSlave<P2PReferences>>(references);
    this->countUniqueServers = countUniqueServers;
    cond.notify_one();
    lock.unlock();
    
    if (!th.joinable()) {
        th = std::thread(&P2PThread::work, this);
    }
}

} // namespace torrent_node_lib
