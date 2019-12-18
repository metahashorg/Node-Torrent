#ifndef TORRENT_NODE_REJECTEDTXSBLOCK_H
#define TORRENT_NODE_REJECTEDTXSBLOCK_H

#include <string>
#include <vector>
#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {

struct RejectedTxsMinimumBlockHeader {
    uint64_t blockSize = 0;

    FilePosition filePos;

    void applyFileNameRelative(const std::string &fileNameRelative);

    size_t endBlockPos() const;
};

struct RejectedTxsBlockHeader {
    uint64_t blockSize = 0;

    size_t timestamp;

    FilePosition filePos;

    std::vector<unsigned char> hash;

    std::vector<unsigned char> prevHash;
    std::vector<unsigned char> txsSign;
    std::vector<unsigned char> txsPubkey;
};

struct RejectedTransactionInfo {
    std::vector<unsigned char> hash;
    size_t error;
};

struct RejectedTxsBlockInfo {
    RejectedTxsBlockHeader header;

    std::vector<RejectedTransactionInfo> txs;
};

}

#endif //TORRENT_NODE_REJECTEDTXSBLOCK_H
