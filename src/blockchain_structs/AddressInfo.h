#ifndef TORRENT_NODE_ADDRESSINFO_H
#define TORRENT_NODE_ADDRESSINFO_H

#include <string>
#include <vector>
#include <optional>
#include "duration.h"
#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {

struct AddressInfo {

    AddressInfo() = default;

    AddressInfo(size_t pos, const std::string &fileName, size_t blockNumber, size_t index)
        : filePos(fileName, pos)
        , blockNumber(blockNumber)
        , blockIndex(index)
    {}

    torrent_node_lib::FilePosition filePos;

    size_t blockNumber = 0;
    size_t blockIndex = 0;

    std::optional<int64_t> undelegateValue;

    void serialize(std::vector<char> &buffer) const;

    static AddressInfo deserialize(const std::string &raw);
};

}

#endif //TORRENT_NODE_ADDRESSINFO_H
