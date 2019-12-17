#include "DelegateState.h"

#include <check.h>
#include <utils/serialize.h>

namespace torrent_node_lib {
void DelegateState::serialize(std::vector<char> &buffer) const {
    CHECK(!hash.empty(), "DelegateState not initialized");
    serializeInt<size_t>(value, buffer);
    serializeString(hash, buffer);
}

DelegateState DelegateState::deserialize(const std::string &raw) {
    DelegateState state;
    if (raw.empty()) {
        return state;
    }

    size_t from = 0;
    state.value = deserializeInt<size_t>(raw, from);
    state.hash = deserializeString(raw, from);
    return state;
}
}