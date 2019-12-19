#ifndef TORRENT_NODE_REJECTEDBLOCKSOURCE_H
#define TORRENT_NODE_REJECTEDBLOCKSOURCE_H

#include <string>

#include "blockchain_structs/RejectedTxsBlock.h"

namespace torrent_node_lib {

struct RejectedBlockResult {
    std::vector<unsigned char> hash;
    size_t blockNumber;
    size_t timestamp;

    RejectedBlockResult(const std::vector<unsigned char> &hash, size_t blockNumber, size_t timestamp)
        : hash(hash)
        , blockNumber(blockNumber)
        , timestamp(timestamp)
    {}
};

struct RejectedBlock {
    RejectedTxsBlockInfo block;
    size_t blockNumber;

    RejectedBlock(const RejectedTxsBlockInfo &block, size_t blockNumber)
        : block(block)
        , blockNumber(blockNumber)
    {}
};

class RejectedBlockSource {
public:

    virtual ~RejectedBlockSource() = default;

public:

    virtual std::vector<RejectedBlockResult> calcLastBlocks(size_t count) = 0;

    virtual std::vector<RejectedBlock> getBlocks(const std::vector<std::vector<unsigned char>> &hashes) const = 0;

};

} // namespace torrent_node_lib {

#endif //TORRENT_NODE_REJECTEDBLOCKSOURCE_H
