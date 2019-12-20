#ifndef TORRENT_NODE_GETNEWREJECTEDBLOCKSFROMSERVER_H
#define TORRENT_NODE_GETNEWREJECTEDBLOCKSFROMSERVER_H

#include "P2P/P2P.h"

#include "NetworkRejectedBlockSourceStructs.h"

namespace torrent_node_lib {

class GetNewRejectedBlocksFromServer {
public:

    GetNewRejectedBlocksFromServer(P2P &p2p, bool isCompress)
        : p2p(p2p)
        , isCompress(isCompress)
    {}

public:

    static ResponseParse parseDumpBlockResponse(bool isCompress, const std::string& result, size_t fromByte, size_t toByte);

public:

    std::vector<RejectedBlockMessage> getLastRejectedBlocks(size_t countLast);

    std::vector<std::string> getRejectedBlocksDumps(const std::vector<std::vector<unsigned char>> &hashes);

private:

    P2P &p2p;

    bool isCompress;
};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_GETNEWREJECTEDBLOCKSFROMSERVER_H
