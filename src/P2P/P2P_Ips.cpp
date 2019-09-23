#include "P2P_Ips.h"

#include "curlWrapper.h"

#include "check.h"
#include "log.h"

#include "parallel_for.h"

using namespace common;

namespace torrent_node_lib {

const static size_t SIZE_PARALLEL_BROADCAST = 8;
    
P2P_Ips::P2P_Ips(const std::vector<std::string> &servers, size_t countConnections)
    : P2P(countConnections * servers.size())
    , servers(servers.begin(), servers.end())
    , countConnections(countConnections)
{
    CHECK(countConnections != 0, "Incorrect count connections: 0");
    Curl::initialize();
    
    for (size_t i = 0; i < SIZE_PARALLEL_BROADCAST; i++) {
        curlsBroadcast.emplace_back(Curl::getInstance());
    }
}

P2P_Ips::~P2P_Ips() = default;

void P2P_Ips::broadcast(const std::string &qs, const std::string &postData, const std::string &header, const BroadcastResult& callback) const {
    parallelFor(SIZE_PARALLEL_BROADCAST, servers.begin(), servers.end(), [&qs, &postData, &header, &callback, this](size_t threadNum, const Server &server) {
        try {
            const std::string response = P2P::request(curlsBroadcast.at(threadNum), qs, postData, header, server.server);
            callback(server.server, response, {});
        } catch (const exception &e) {
            callback(server.server, "", CurlException(e));
            return;
        }
        
        checkStopSignal();
    });
    
    checkStopSignal();
}

std::string P2P_Ips::runOneRequest(const std::string& server, const std::string& qs, const std::string& postData, const std::string& header) const {
    return P2P::request(Curl::getInstance(), qs, postData, header, server);
}

size_t P2P_Ips::getMaxServersCount(const std::vector<Server> &srvrs) const {
    return countConnections * srvrs.size();
}

std::vector<P2P::ThreadDistribution> P2P_Ips::getServersList(const std::vector<Server> &srves, size_t countSegments) const {
    std::vector<P2P::ThreadDistribution> result;
    
    // TODO сейчас эта функция не принимает в рассчет предыдущее распределение серверов по потокам, что нехорошо
    
    const size_t currConnections = std::min((countSegments + srves.size() - 1) / srves.size(), countConnections);
    
    size_t curlIndex = 0;
    for (size_t i = 0; i < currConnections; i++) {
        for (const Server &server: srves) {
            result.emplace_back(curlIndex, curlIndex + 1, server.server); // TODO оптимизировать
            curlIndex++;
        }
        
        if (curlIndex >= countSegments) {
            break;
        }
    }
    return result;
}

std::vector<std::string> P2P_Ips::requestImpl(size_t responseSize, size_t minResponseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &/*hintsServers*/) {
    CHECK(responseSize != 0, "response size 0");
    
    const bool isMultitplyRequests = minResponseSize == 1;
    
    const size_t maxServers = getMaxServersCount(servers);
    
    const size_t countSegments = isMultitplyRequests ? responseSize : std::min((responseSize + minResponseSize - 1) / minResponseSize, maxServers);
    const auto requestServers = getServersList(servers, countSegments);
    
    std::vector<std::string> answers(countSegments);
    std::mutex answersMut;

    const ProcessResponse processResponse = [&answers, &answersMut, &header, &responseParse, isPrecisionSize, this](const std::string &response, const Segment &segment) {        
        const ResponseParse parsed = responseParse(response, segment.fromByte, segment.toByte);
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

std::string P2P_Ips::request(size_t responseSize, bool isPrecisionSize, const MakeQsAndPostFunction& makeQsAndPost, const std::string& header, const ResponseParseFunction& responseParse, const std::vector<std::string> &hintsServers) {
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

std::vector<std::string> P2P_Ips::requests(size_t countRequests, const MakeQsAndPostFunction2 &makeQsAndPost, const std::string &header, const ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) {
    const auto makeQsAndPortImpl = [makeQsAndPost](size_t from, size_t to) {
        CHECK(to == from + 1, "Incorrect call makeQsAndPortFunc");
        return makeQsAndPost(from);
    };
    const std::vector<std::string> answers = requestImpl(countRequests, 1, false, makeQsAndPortImpl, header, responseParse, hintsServers);
    CHECK(answers.size() == countRequests, "Incorrect count answers");
    return answers;
}

SendAllResult P2P_Ips::requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const {
    const auto &mainElementsGraph = servers;
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

}
