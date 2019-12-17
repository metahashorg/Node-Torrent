#ifndef TORRENT_NODE_MAINBLOCKINFO_H
#define TORRENT_NODE_MAINBLOCKINFO_H

#include <string>
#include <vector>

namespace torrent_node_lib {
struct MainBlockInfo {
    size_t blockNumber = 0;
    std::vector<unsigned char> blockHash;
    size_t countVal = 0;

    MainBlockInfo() = default;

    MainBlockInfo(size_t blockNumber, const std::vector<unsigned char> &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}

    std::string serialize() const;

    static MainBlockInfo deserialize(const std::string &raw);

};
}

#endif //TORRENT_NODE_MAINBLOCKINFO_H
