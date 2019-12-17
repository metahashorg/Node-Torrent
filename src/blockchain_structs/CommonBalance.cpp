#include "CommonBalance.h"

#include "stringUtils.h"
#include "check.h"
#include <utils/serialize.h>

namespace torrent_node_lib {
void CommonBalance::serialize(std::vector<char>& buffer) const {
    serializeInt<size_t>(money, buffer);
    serializeInt<size_t>(blockNumber, buffer);
}

CommonBalance CommonBalance::deserialize(const std::string& raw) {
    CommonBalance result;

    if (raw.empty()) {
        return result;
    }

    size_t from = 0;
    result.money = deserializeInt<size_t>(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);

    return result;
}

std::string ForgingSums::serialize() const {
    std::string res;
    res += serializeInt(sums.size());
    for (const auto &[key, value]: sums) {
        res += serializeInt(key);
        res += serializeInt(value);
    }
    res += serializeInt(blockNumber);
    return res;
}

ForgingSums ForgingSums::deserialize(const std::string &raw) {
    ForgingSums state;
    if (raw.empty()) {
        return state;
    }

    size_t from = 0;
    const size_t size = deserializeInt<size_t>(raw, from);
    for (size_t i = 0; i < size; i++) {
        const uint64_t key = deserializeInt<size_t>(raw, from);
        const uint64_t value = deserializeInt<size_t>(raw, from);
        state.sums.emplace(key, value);
    }
    state.blockNumber = deserializeInt<size_t>(raw, from);
    return state;
}

ForgingSums& ForgingSums::operator+=(const ForgingSums &second) {
    for (const auto &[key, value]: second.sums) {
        this->sums[key] += value;
    }
    this->blockNumber = std::max(this->blockNumber, second.blockNumber);
    return *this;
}
}