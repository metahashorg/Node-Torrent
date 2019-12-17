#ifndef TORRENT_NODE_REJECTEDTXSBLOCK_H
#define TORRENT_NODE_REJECTEDTXSBLOCK_H

#include <string>
#include <vector>
#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {
struct RejectedTxsBlockHeader {
    uint64_t blockSize = 0;

    FilePosition filePos;

    size_t endBlockPos() const;
};

struct RejectedTxsBlockInfo {
    RejectedTxsBlockHeader header;
};
}

#endif //TORRENT_NODE_REJECTEDTXSBLOCK_H
