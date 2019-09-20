#ifndef P2P_STRUCTS_H_
#define P2P_STRUCTS_H_

#include <functional>
#include <string>

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

    
using MakeQsAndPostFunction = std::function<std::pair<std::string, std::string>(size_t fromByte, size_t toByte)>;
    
using ProcessResponse = std::function<bool(const std::string &response, const Segment &segment)>;

struct P2PReferences {
    
    P2PReferences(const MakeQsAndPostFunction &makeQsAndPost, const ProcessResponse &processResponse)
        : makeQsAndPost(makeQsAndPost)
        , processResponse(processResponse)
    {}
    
    const MakeQsAndPostFunction &makeQsAndPost;
    const ProcessResponse &processResponse;
};

} // namespace torrent_node_lib

#endif // P2P_STRUCTS_H_
