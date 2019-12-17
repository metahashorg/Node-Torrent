#ifndef TORRENT_NODE_DELEGATESTATE_H
#define TORRENT_NODE_DELEGATESTATE_H

#include <string>
#include <vector>

namespace torrent_node_lib {
struct DelegateState {
    int64_t value = 0;

    std::string hash;

    DelegateState() = default;

    DelegateState(int64_t value, const std::string &hash)
        : value(value)
        , hash(hash)
    {}

    void serialize(std::vector<char> &buffer) const;

    static DelegateState deserialize(const std::string &raw);
};
}

#endif //TORRENT_NODE_DELEGATESTATE_H
