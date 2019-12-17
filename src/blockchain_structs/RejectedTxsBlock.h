#ifndef TORRENT_NODE_REJECTEDTXSBLOCK_H
#define TORRENT_NODE_REJECTEDTXSBLOCK_H

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <set>
#include <unordered_map>
#include "duration.h"
#include "blockchain_structs/Address.h"
#include "blockchain_structs/FilePosition.h"
#include "blockchain_structs/TransactionInfo.h"

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
