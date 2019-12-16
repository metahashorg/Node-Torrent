#include "AddressInfo.h"

#include <check.h>
#include <utils/serialize.h>
#include <blockchain_structs/FilePosition.h>

namespace torrent_node_lib {

void AddressInfo::serialize(std::vector<char>& buffer) const {
    CHECK(blockNumber != 0, "AddressInfo not initialized");

    filePos.serialize(buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeInt<size_t>(blockIndex, buffer);
    if (undelegateValue.has_value()) {
        serializeInt<uint64_t>(undelegateValue.value(), buffer);
    }
}

AddressInfo AddressInfo::deserialize(const std::string& raw) {
    AddressInfo result;

    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.blockIndex = deserializeInt<size_t>(raw, from);
    if (from < raw.size()) {
        result.undelegateValue = deserializeInt<uint64_t>(raw, from);
    }

    return result;
}

}
