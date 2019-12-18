#include "BlockInfo.h"

#include "check.h"
#include "log.h"
#include "utils/serialize.h"
#include "stringUtils.h"
#include "blockchain_structs/AddressInfo.h"
#include "blockchain_structs/TransactionInfo.h"

using namespace common;

namespace torrent_node_lib {

const static uint64_t BLOCK_TYPE = 0xEFCDAB8967452301;
const static uint64_t BLOCK_TYPE_COMMON = 0x0000000067452301;
const static uint64_t BLOCK_TYPE_STATE = 0x1100000067452301;
const static uint64_t BLOCK_TYPE_FORGING = 0x2200000067452301;

const static uint64_t BLOCK_TYPE_COMMON_2 =  0x0001000067452301;
const static uint64_t BLOCK_TYPE_STATE_2 =   0x1101000067452301;
const static uint64_t BLOCK_TYPE_FORGING_2 = 0x2201000067452301;

bool BlockHeader::isStateBlock() const {
    return blockType == BLOCK_TYPE_STATE || blockType == BLOCK_TYPE_STATE_2;
}

bool BlockHeader::isSimpleBlock() const {
    return blockType == BLOCK_TYPE || blockType == BLOCK_TYPE_COMMON || blockType == BLOCK_TYPE_COMMON_2;
}

bool BlockHeader::isForgingBlock() const {
    return blockType == BLOCK_TYPE_FORGING || blockType == BLOCK_TYPE_FORGING_2;
}

std::string BlockHeader::getBlockType() const {
    if (isStateBlock()) {
        return "state";
    } else if (isSimpleBlock()) {
        return "block";
    } else if (isForgingBlock()) {
        return "forging";
    } else {
        throwErr("Unknown block type " + std::to_string(blockType));
    }
}

size_t BlockHeader::endBlockPos() const {
    CHECK(blockSize != 0, "Incorrect block size");
    return filePos.pos + blockSize + sizeof(uint64_t);
}

std::string BlockHeader::serialize() const {
    CHECK(!hash.empty(), "empty hash");
    CHECK(!prevHash.empty(), "empty prevHash");
    CHECK(!txsHash.empty(), "empty txsHash");
    CHECK(blockSize != 0, "BlockHeader not initialized");
    CHECK(blockType != 0, "BlockHeader not initialized");
    
    std::string res;
    res += filePos.serialize();
    res += serializeVector(prevHash);
    res += serializeVector(hash);
    res += serializeVector(txsHash);
    res += serializeString(std::string(signature.begin(), signature.end()));
    res += serializeInt<uint64_t>(blockSize);
    res += serializeInt<uint64_t>(blockType);
    res += serializeInt<size_t>(timestamp);
    res += serializeInt<size_t>(countTxs);
    res += serializeInt<size_t>(countSignTx);
    
    res += serializeString(std::string(senderSign.begin(), senderSign.end()));
    res += serializeString(std::string(senderPubkey.begin(), senderPubkey.end()));
    res += serializeString(std::string(senderAddress.begin(), senderAddress.end()));
    
    return res;
}

BlockHeader BlockHeader::deserialize(const std::string& raw) {
    BlockHeader result;
    
    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.prevHash = deserializeVector(raw, from);
    result.hash = deserializeVector(raw, from);
    result.txsHash = deserializeVector(raw, from);
    const std::string sign = deserializeString(raw, from);
    result.signature = std::vector<unsigned char>(sign.begin(), sign.end());
    result.blockSize = deserializeInt<uint64_t>(raw, from);
    result.blockType = deserializeInt<uint64_t>(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.countTxs = deserializeInt<size_t>(raw, from);
    result.countSignTx = deserializeInt<size_t>(raw, from);
    
    const std::string senderSign = deserializeString(raw, from);
    result.senderSign = std::vector<unsigned char>(senderSign.begin(), senderSign.end());
    const std::string senderPubkey = deserializeString(raw, from);
    result.senderPubkey = std::vector<unsigned char>(senderPubkey.begin(), senderPubkey.end());
    const std::string senderAddress = deserializeString(raw, from);
    result.senderAddress = std::vector<unsigned char>(senderAddress.begin(), senderAddress.end());
    
    return result;
}

void BlockInfo::applyFileNameRelative(const std::string &fileNameRelative) {
    header.filePos.fileNameRelative = fileNameRelative;
    for (auto &tx : txs) {
        tx.filePos.fileNameRelative = fileNameRelative;
    }
}

std::vector<TransactionInfo> BlockInfo::getBlockSignatures() const {
    std::vector<TransactionInfo> signatures;
    std::copy_if(txs.begin(), txs.end(), std::back_inserter(signatures), [](const TransactionInfo &info) {
        return info.isSignBlockTx;
    });
    return signatures;
}

}
