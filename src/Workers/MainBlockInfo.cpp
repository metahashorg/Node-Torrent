#include <check.h>
#include <utils/serialize.h>
#include "MainBlockInfo.h"

namespace torrent_node_lib {
std::string MainBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(blockNumber != 0, "MainBlockInfo not initialized");
    
    std::string res;
    res += serializeVector(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

MainBlockInfo MainBlockInfo::deserialize(const std::string& raw) {
    MainBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}
}