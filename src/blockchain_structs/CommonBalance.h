#ifndef TORRENT_NODE_COMMONBALANCE_H
#define TORRENT_NODE_COMMONBALANCE_H

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

struct CommonBalance {
    size_t money = 0;
    size_t blockNumber = 0;

    CommonBalance() = default;

    void serialize(std::vector<char> &buffer) const;

    static CommonBalance deserialize(const std::string &raw);
};

struct ForgingSums {
    std::unordered_map<uint64_t, size_t> sums;
    size_t blockNumber = 0;

    std::string serialize() const;

    static ForgingSums deserialize(const std::string &raw);

    ForgingSums& operator +=(const ForgingSums &second);

};
}

#endif //TORRENT_NODE_COMMONBALANCE_H
