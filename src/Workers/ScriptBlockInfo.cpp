#include "ScriptBlockInfo.h"

#include "utils/serialize.h"
#include "check.h"

namespace torrent_node_lib {
    
std::string ScriptBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(blockNumber != 0, "ScriptBlockInfo not initialized");
    
    std::string res;
    res += serializeVector(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

ScriptBlockInfo ScriptBlockInfo::deserialize(const std::string& raw) {
    ScriptBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}

std::string V8State::serialize() const {
    CHECK(blockNumber != 0, "V8State not initialized");
    
    std::string res;
    res += serializeString(state);
    res += serializeString(details);
    res += serializeInt<size_t>(blockNumber);
    return res;
}

V8State V8State::deserialize(const std::string &raw) {
    V8State state;
    if (raw.empty()) {
        return state;
    }
    
    size_t from = 0;
    state.state = deserializeString(raw, from);
    state.details = deserializeString(raw, from);
    state.blockNumber = deserializeInt<size_t>(raw, from);
    return state;
}

std::string V8Details::serialize() const {
    std::string res;
    res += serializeString(details);
    res += serializeString(lastError);
    return res;
}

V8Details V8Details::deserialize(const std::string &raw) {
    V8Details state;
    if (raw.empty()) {
        return state;
    }
    
    size_t from = 0;
    state.details = deserializeString(raw, from);
    state.lastError = deserializeString(raw, from);
    return state;
}

std::string V8Code::serialize() const {
    std::string res;
    res += serializeString(std::string(code.begin(), code.end()));
    return res;
}

V8Code V8Code::deserialize(const std::string &raw) {
    V8Code state;
    if (raw.empty()) {
        return state;
    }
    
    size_t from = 0;
    const std::string code = deserializeString(raw, from);
    state.code = std::vector<unsigned char>(code.begin(), code.end());
    return state;
}

}
