#ifndef TORRENT_NODE_BLOCKSMETADATA_H
#define TORRENT_NODE_BLOCKSMETADATA_H

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
struct BlocksMetadata {
    std::vector<unsigned char> blockHash;
    std::vector<unsigned char> prevBlockHash;

    BlocksMetadata() = default;

    BlocksMetadata(const std::vector<unsigned char> &blockHash, const std::vector<unsigned char> &prevBlockHash)
        : blockHash(blockHash)
        , prevBlockHash(prevBlockHash)
    {}

    std::string serialize() const;

    static BlocksMetadata deserialize(const std::string &raw);

};

struct FileInfo {

    FilePosition filePos;

    FileInfo()
        : filePos("", 0)
    {}

    std::string serialize() const;

    static FileInfo deserialize(const std::string &raw);

};
}

#endif //TORRENT_NODE_BLOCKSMETADATA_H
