#include "P2P_Graph.h"

#include "curlWrapper.h"

#include "check.h"
#include "log.h"

#include "stopProgram.h"
#include "parallel_for.h"

using namespace common;
using namespace torrent_node_lib;

P2P_Graph::P2P_Graph(const std::vector<std::pair<std::string, std::string>> &graphVec, const std::string &thisIp, size_t countConnections)
    : P2P(countConnections)
    , countConnections(countConnections)
{
    CHECK(countConnections != 0, "Incorrect count connections: 0");
    Curl::initialize();
    for (const auto &[parent, child]: graphVec) {
        graph.addEdge(parent, child);
    }
    
    const GraphString::Element &thisElement = graph.findElement(thisIp);
    parent = &thisElement.getParent();
    
    LOGINFO << "Found parent on this: " << parent->getElement();
}

void P2P_Graph::broadcast(const std::string &qs, const std::string &postData, const std::string &header, const BroadcastResult& callback) const {
    const GraphString::Element *curServ = parent;
    while (true) {
        try {
            const std::string response = P2P::request(curlBroadcast, qs, postData, header, curServ->getElement());
            callback(curServ->getElement(), response, {});
            break;
        } catch (const exception &e) {
            callback(curServ->getElement(), "", CurlException(e));
        }
        
        checkStopSignal();
        
        const std::string &prevServer = curServ->getElement();
        CHECK(curServ->isParent(), "Servers end");
        curServ = &curServ->getParent();
        LOGWARN << "Server " << prevServer << " not response. Found next server " << curServ->getElement();
    };
    
    checkStopSignal();
}

std::string P2P_Graph::runOneRequest(const std::string& server, const std::string& qs, const std::string& postData, const std::string& header) const {
    return P2P::request(curlBroadcast, qs, postData, header, server);
}

size_t P2P_Graph::getMaxServersCount(const Server &srvr) const {
    return countConnections;
}

std::vector<P2P::ThreadDistribution> P2P_Graph::getServersList(const Server &server, size_t countSegments) const {
    std::vector<P2P::ThreadDistribution> result;
    result.emplace_back(0, std::min(countConnections, countSegments), server.server);
    return result;
}

std::vector<std::string> P2P_Graph::requestImpl(size_t responseSize, size_t minResponseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) {
    CHECK(responseSize != 0, "response size 0");
    
    const bool isMultitplyRequests = minResponseSize == 1;
        
    CHECK(hintsServers.size() == 1, "Incorrect hint servers");
    const Server server(hintsServers[0]); // Этот объект не должен пропасть до конца действия процедуры, так как на него берется ссылка
    
    const size_t maxServers = getMaxServersCount(server);
    
    const size_t countSegments = isMultitplyRequests ? responseSize : std::min((responseSize + minResponseSize - 1) / minResponseSize, maxServers);
    const auto requestServers = getServersList(server, countSegments);
    
    std::vector<std::string> answers(countSegments);
    std::mutex answersMut;
    
    const ProcessResponse processResponse = [&answers, &answersMut, &header, &responseParse, isPrecisionSize](const std::string &response, const Segment &segment) {
        const ResponseParse parsed = responseParse(response);
        CHECK(!parsed.error.has_value(), parsed.error.value());
        if (isPrecisionSize) {
            CHECK(parsed.response.size() == segment.toByte - segment.fromByte, "Incorrect response size");
        }
        
        std::lock_guard<std::mutex> lock(answersMut);
        if (answers.at(segment.posInArray).empty()) {
            answers.at(segment.posInArray) = parsed.response;
            return true;
        } else {
            return false;
        }
    };
    
    const std::vector<Segment> segments = P2P::makeSegments(countSegments, responseSize, minResponseSize);
    const bool isSuccess = P2P::process(requestServers, segments, makeQsAndPost, processResponse);
    CHECK(isSuccess, "dont run request");
        
    return answers;
}

std::string P2P_Graph::request(size_t responseSize, bool isPrecisionSize, const MakeQsAndPostFunction& makeQsAndPost, const std::string& header, const ResponseParseFunction& responseParse, const std::vector<std::string> &hintsServers) {
    const size_t MIN_RESPONSE_SIZE = 10000;
    
    const std::vector<std::string> answers = requestImpl(responseSize, MIN_RESPONSE_SIZE, isPrecisionSize, makeQsAndPost, header, responseParse, hintsServers);
    
    std::string response;
    response.reserve(responseSize);
    for (const std::string &answer: answers) {
        response += answer;
    }
    if (isPrecisionSize) {
        CHECK(response.size() == responseSize, "response size != getted response. Getted response size: " + std::to_string(response.size()) + ". Expected response size: " + std::to_string(responseSize));
    }
    
    return response;
}

std::vector<std::string> P2P_Graph::requests(size_t countRequests, const MakeQsAndPostFunction2 &makeQsAndPost, const std::string &header, const ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) {
    const auto makeQsAndPortImpl = [makeQsAndPost](size_t from, size_t to) {
        CHECK(to == from + 1, "Incorrect call makeQsAndPortFunc");
        return makeQsAndPost(from);
    };
    const std::vector<std::string> answers = requestImpl(countRequests, 1, false, makeQsAndPortImpl, header, responseParse, hintsServers);
    CHECK(answers.size() == countRequests, "Incorrect count answers");
    return answers;
}

SendAllResult P2P_Graph::requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const {
    const auto &mainElementsGraph = graph.getAllElements();
    //const std::vector<Server> mainServers(mainElementsGraph.begin(), mainElementsGraph.end());
    const std::vector<Server> otherServers(additionalServers.begin(), additionalServers.end());
    std::vector<std::reference_wrapper<const Server>> allServers;
    //allServers.insert(allServers.end(), mainServers.begin(), mainServers.end());
    allServers.insert(allServers.end(), otherServers.begin(), otherServers.end());
    
    const auto requestFunction = [this](const std::string &qs, const std::string &post, const std::string &header, const std::string &server) -> std::string {
        return P2P::request(Curl::getInstance(), qs, post, header, server);
    };
    
    return P2P::process(allServers, qs, postData, header, requestFunction);
}
