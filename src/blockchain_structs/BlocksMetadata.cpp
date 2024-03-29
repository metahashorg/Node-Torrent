#include "blockchain_structs/BlocksMetadata.h"

#include "check.h"
#include "utils/serialize.h"

namespace torrent_node_lib {

std::string BlocksMetadata::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(!prevBlockHash.empty(), "Incorrect metadata");

    std::string res;
    res += serializeVector(blockHash);
    res += serializeVector(prevBlockHash);
    return res;
}

BlocksMetadata BlocksMetadata::deserialize(const std::string& raw) {
    BlocksMetadata result;

    if (raw.empty()) {
        return result;
    }

    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.prevBlockHash = deserializeVector(raw, from);
    return result;
}

std::string FileInfo::serialize() const {
    return filePos.serialize();
}

FileInfo FileInfo::deserialize(const std::string& raw) {
    FileInfo result;

    result.filePos = FilePosition::deserialize(raw);

    return result;
}
}
