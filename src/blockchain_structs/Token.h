#ifndef TORRENT_NODE_TOKEN_H
#define TORRENT_NODE_TOKEN_H

#include <string>
#include <vector>
#include <optional>
#include "blockchain_structs/Address.h"
#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {

struct Token {
    std::string type;
    Address owner;
    int decimals;
    size_t beginValue;
    size_t allValue;
    std::string symbol;
    std::string name;
    bool emission;
    std::string txHash;

    void serialize(std::vector<char> &buffer) const;

    static Token deserialize(const std::string &raw);

};

} // namespace torrent_node_lib {

#endif //TORRENT_NODE_TOKEN_H
