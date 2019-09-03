#include "P2P_Ips.h"

#include "curlWrapper.h"

#include "check.h"
#include "log.h"

#include "parallel_for.h"

using namespace common;

namespace torrent_node_lib {

P2P_Ips::P2P_Ips(const std::vector<std::string> &servers, size_t countConnections)
    : servers(servers.begin(), servers.end())
    , countConnections(countConnections)
{
    CHECK(countConnections != 0, "Incorrect count connections: 0");
    Curl::initialize();
    
    for (size_t j = 0; j < servers.size(); j++) {
        for (size_t i = 0; i < countConnections; i++) {
            curls.emplace_back(Curl::getInstance());
        }
    }
}

P2P_Ips::~P2P_Ips() = default;

std::string P2P_Ips::request(const CurlInstance &curl, const std::string& qs, const std::string& postData, const std::string& header, const std::string& server) const {
    std::string url = server;
    CHECK(!url.empty(), "server empty");
    if (url[url.size() - 1] != '/') {
        url += '/';
    }
    url += qs;
    const std::string response = Curl::request(curl, url, postData, header, "", 5);
    return response;
}

void P2P_Ips::broadcast(const std::string &qs, const std::string &postData, const std::string &header, const BroadcastResult& callback) const {
    parallelFor(8, servers.begin(), servers.end(), [&qs, &postData, &header, &callback, this](const Server &server) {
        try {
            const std::string response = request(Curl::getInstance(), qs, postData, header, server.server);
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
    return request(Curl::getInstance(), qs, postData, header, server);
}

std::vector<std::pair<std::reference_wrapper<const P2P_Ips::Server>, std::reference_wrapper<const common::CurlInstance>>> P2P_Ips::getServersList(const std::vector<Server> &srves) const {
    std::vector<std::pair<std::reference_wrapper<const P2P_Ips::Server>, std::reference_wrapper<const common::CurlInstance>>> result;
    size_t curlIndex = 0;
    for (size_t i = 0; i < countConnections; i++) {
        CHECK(curls.size() >= curlIndex + srves.size(), "Incorrect curls size");
        for (const Server &server: srves) {
            result.emplace_back(server, curls[curlIndex]);
            curlIndex++;
        }
    }
    return result;
}

std::vector<std::string> P2P_Ips::requestImpl(size_t responseSize, size_t minResponseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &/*hintsServers*/) const {
    CHECK(responseSize != 0, "response size 0");
    
    const auto requestServers = getServersList(servers);
    
    const bool isMultitplyRequests = minResponseSize == 1;
    const size_t countSegments = isMultitplyRequests ? responseSize : std::min((responseSize + minResponseSize - 1) / minResponseSize, requestServers.size());
    
    std::vector<std::string> answers(countSegments);
        
    const RequestFunction requestFunction = [&answers, &header, &responseParse, isPrecisionSize, this](const std::string &qs, const std::string &post, const std::string &server, const common::CurlInstance &curl, const Segment &segment) {        
        const std::string response = request(curl, qs, post, header, server);
        const ResponseParse parsed = responseParse(response);
        CHECK(!parsed.error.has_value(), parsed.error.value());
        if (isPrecisionSize) {
            CHECK(parsed.response.size() == segment.toByte - segment.fromByte, "Incorrect response size");
        }
        answers.at(segment.posInArray) = parsed.response;
    };
    
    const std::vector<Segment> segments = P2P::makeSegments(countSegments, responseSize, minResponseSize);
    const bool isSuccess = P2P::process(requestServers, segments, makeQsAndPost, requestFunction);
    
    CHECK(isSuccess, "dont run request");
    
    return answers;
}

std::string P2P_Ips::request(size_t responseSize, bool isPrecisionSize, const MakeQsAndPostFunction& makeQsAndPost, const std::string& header, const ResponseParseFunction& responseParse, const std::vector<std::string> &hintsServers) const {
    const size_t MIN_RESPONSE_SIZE = 1000;
    
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

std::vector<std::string> P2P_Ips::requests(size_t countRequests, const MakeQsAndPostFunction2 &makeQsAndPost, const std::string &header, const ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const {
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
        return request(Curl::getInstance(), qs, post, header, server);
    };
    
    return P2P::process(allServers, qs, postData, header, requestFunction);
}

}
