#include "Token.h"

#include "utils/serialize.h"

namespace torrent_node_lib {
void Token::serialize(std::vector<char> &buffer) const {
    serializeString(type, buffer);
    serializeString(owner.toBdString(), buffer);
    serializeInt<unsigned int>(decimals, buffer);
    serializeInt(beginValue, buffer);
    serializeInt(allValue, buffer);
    serializeString(symbol, buffer);
    serializeString(name, buffer);
    serializeInt<unsigned char>(emission ? 1 : 0, buffer);
    serializeString(txHash, buffer);
}

Token Token::deserialize(const std::string &raw) {
    Token result;
    if (raw.empty()) {
        return result;
    }
    size_t from = 0;
    result.type = deserializeString(raw, from);
    const std::string owner = deserializeString(raw, from);
    result.owner = Address(owner.begin(), owner.end());
    result.decimals = deserializeInt<unsigned int>(raw, from);
    result.beginValue = deserializeInt<size_t>(raw, from);
    result.allValue = deserializeInt<size_t>(raw, from);
    result.symbol = deserializeString(raw, from);
    result.name = deserializeString(raw, from);
    result.emission = deserializeInt<unsigned char>(raw, from) == 1;
    result.txHash = deserializeString(raw, from);

    return result;
}
}