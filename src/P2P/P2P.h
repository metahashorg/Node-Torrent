#ifndef P2P_H_
#define P2P_H_

#include <vector>
#include <string>
#include <set>
#include <functional>
#include <optional>
#include <mutex>

#include "duration.h"

#include "QueueP2P.h"

namespace common {
struct CurlInstance;
}

namespace torrent_node_lib {

struct CurlException {
    
    CurlException(const std::string &message)
        : message(message)
    {}
    
    const std::string message;
    
};

using BroadcastResult = std::function<void(const std::string &server, const std::string &result, const std::optional<CurlException> &exception)>;

using MakeQsAndPostFunction = std::function<std::pair<std::string, std::string>(size_t fromByte, size_t toByte)>;

using MakeQsAndPostFunction2 = std::function<std::pair<std::string, std::string>(size_t number)>;

using MakeQsAndPostFunctionSimple = std::function<std::pair<std::string, std::string>()>;

struct ResponseParse {
    std::string response;
    std::optional<std::string> error;
};

using ResponseParseFunction = std::function<ResponseParse(const std::string &result)>;

struct SendAllResult {
    struct SendOneResult {
        std::string server;
        ResponseParse response;
        milliseconds time;
        
        SendOneResult(const std::string &server, const ResponseParse &response, const milliseconds &time)
            : server(server)
            , response(response)
            , time(time)
        {}
    };
    
    std::vector<SendOneResult> results;
};

struct P2PReferences;

class P2P {   
    friend struct P2PReferences;
public:
    
    virtual ~P2P() = default;
    
    /**
     *c Выполняет запрос по всем серверам. Результаты возвращает в callback.
     *c callback должен быть готов к тому, что его будут вызывать из нескольких потоков.
     */
    virtual void broadcast(const std::string &qs, const std::string &postData, const std::string &header, const BroadcastResult &callback) const = 0;
    
    virtual std::string request(size_t responseSize, bool isPrecisionSize, const MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const = 0;
    
    virtual std::vector<std::string> requests(size_t countRequests, const MakeQsAndPostFunction2 &makeQsAndPost, const std::string &header, const ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const = 0;
    
    virtual std::string runOneRequest(const std::string &server, const std::string &qs, const std::string &postData, const std::string &header) const = 0;
    
    virtual SendAllResult requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const = 0;
    
protected:
    
    struct Server {
        std::string server;
        
        Server(const std::string &server)
            : server(server)
        {}
    };
        
    using ProcessResponse = std::function<bool(const std::string &response, const Segment &segment)>;
    
    using RequestFunctionSimple = std::function<std::string(const std::string &qs, const std::string &post, const std::string &header, const std::string &server)>;
        
    static std::vector<Segment> makeSegments(size_t countSegments, size_t size, size_t minSize);
    
    static std::string request(const common::CurlInstance &curl, const std::string &qs, const std::string &postData, const std::string &header, const std::string &server);
    
    static bool process(const std::vector<std::pair<std::reference_wrapper<const Server>, std::reference_wrapper<const common::CurlInstance>>> &requestServers, const std::vector<Segment> &segments, const MakeQsAndPostFunction &makeQsAndPost, const ProcessResponse &processResponse);
    
    static SendAllResult process(const std::vector<std::reference_wrapper<const Server>> &requestServers, const std::string &qs, const std::string &post, const std::string &header, const RequestFunctionSimple &requestFunction);
    
};

}

#endif // P2P_H_
