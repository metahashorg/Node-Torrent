#include "SignBlock.h"

#include <check.h>
#include <utils/serialize.h>

namespace torrent_node_lib {

void SignBlockInfo::applyFileNameRelative(const std::string &fileNameRelative) {
    header.filePos.fileNameRelative = fileNameRelative;
}

std::string SignBlockHeader::serialize() const {
    CHECK(!hash.empty(), "empty hash");
    CHECK(!prevHash.empty(), "empty prevHash");
    CHECK(blockSize != 0, "BlockHeader not initialized");
    CHECK(!filePos.fileNameRelative.empty(), "SignBlockHeader not setted fileName");
    
    std::string res;
    res += filePos.serialize();
    res += serializeInt(timestamp);
    res += serializeInt(blockSize);
    
    res += serializeVector(hash);
    res += serializeVector(prevHash);
    
    res += serializeVector(senderSign);
    res += serializeVector(senderPubkey);
    res += serializeVector(senderAddress);
    
    return res;
}

SignBlockHeader SignBlockHeader::deserialize(const std::string &raw) {
    SignBlockHeader result;
    
    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.blockSize = deserializeInt<uint64_t>(raw, from);
    
    result.hash = deserializeVector(raw, from);
    result.prevHash = deserializeVector(raw, from);
    
    result.senderSign = deserializeVector(raw, from);
    result.senderPubkey = deserializeVector(raw, from);
    result.senderAddress = deserializeVector(raw, from);
    
    return result;
}

void MinimumSignBlockHeader::serialize(std::vector<char> &buffer) const {
    CHECK(!hash.empty(), "empty hash");
    CHECK(!filePos.fileNameRelative.empty(), "MinimumSignBlockHeader not setted fileName");
    
    filePos.serialize(buffer);
    serializeVector(hash, buffer);
    serializeVector(prevHash, buffer);
}

MinimumSignBlockHeader MinimumSignBlockHeader::deserialize(const std::string &raw, size_t &fromPos) {
    MinimumSignBlockHeader result;
    
    result.filePos = FilePosition::deserialize(raw, fromPos);
    result.hash = deserializeVector(raw, fromPos);
    result.prevHash = deserializeVector(raw, fromPos);
    
    return result;
}

size_t SignBlockHeader::endBlockPos() const {
    CHECK(blockSize != 0, "Incorrect block size");
    return filePos.pos + blockSize + sizeof(uint64_t);
}

}