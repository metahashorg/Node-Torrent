#include "BlockInfo.h"

#include "check.h"
#include "utils/serialize.h"
#include "convertStrings.h"
#include "stringUtils.h"
#include "blockchain_structs/AddressInfo.h"
#include "blockchain_structs/Token.h"
#include "blockchain_structs/TransactionInfo.h"
#include "blockchain_structs/BalanceInfo.h"
#include <log.h>

using namespace common;

namespace torrent_node_lib {

const std::string Address::INITIAL_WALLET_TRANSACTION = "InitialWalletTransaction";

const static uint64_t BLOCK_TYPE = 0xEFCDAB8967452301;
const static uint64_t BLOCK_TYPE_COMMON = 0x0000000067452301;
const static uint64_t BLOCK_TYPE_STATE = 0x1100000067452301;
const static uint64_t BLOCK_TYPE_FORGING = 0x2200000067452301;

const static uint64_t BLOCK_TYPE_COMMON_2 =  0x0001000067452301;
const static uint64_t BLOCK_TYPE_STATE_2 =   0x1101000067452301;
const static uint64_t BLOCK_TYPE_FORGING_2 = 0x2201000067452301;

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
    return filePos.pos + blockSize + sizeof(uint64_t);
}

size_t SignBlockHeader::endBlockPos() const {
    return filePos.pos + blockSize + sizeof(uint64_t);
}

size_t RejectedTxsBlockHeader::endBlockPos() const {
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

std::vector<TransactionInfo> BlockInfo::getBlockSignatures() const {
    std::vector<TransactionInfo> signatures;
    std::copy_if(txs.begin(), txs.end(), std::back_inserter(signatures), [](const TransactionInfo &info) {
        return info.isSignBlockTx;
    });
    return signatures;
}

std::string BlocksMetadata::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(!prevBlockHash.empty(), "Incorrect metadata");
    
    std::string res;
    res += serializeVector(blockHash);
    res += serializeVector(prevBlockHash);
    return res;
}

BlocksMetadata BlocksMetadata::deserialize(const std::string& raw) {
    BlocksMetadata result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.prevBlockHash = deserializeVector(raw, from);
    return result;
}

std::string MainBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(blockNumber != 0, "MainBlockInfo not initialized");
    
    std::string res;
    res += serializeVector(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

MainBlockInfo MainBlockInfo::deserialize(const std::string& raw) {
    MainBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeVector(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}

std::string FileInfo::serialize() const {
    return filePos.serialize();
}

FileInfo FileInfo::deserialize(const std::string& raw) {
    FileInfo result;
    
    result.filePos = FilePosition::deserialize(raw);
    
    return result;
}

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

size_t getMaxBlockNumber(const std::vector<TransactionInfo> &infos) {
    if (!infos.empty()) {
        const TransactionInfo &first = infos.front();
        const TransactionInfo &last = infos.back();
        const size_t maxBlockNum = std::max(first.blockNumber, last.blockNumber); // хз уже помнит, где находится последняя транзакция, спереди или сзади, так что поверим обе
        return maxBlockNum;
    } else {
        return 0;
    }
}

size_t getMaxBlockNumber(const std::vector<TransactionStatus> &infos) {
    if (!infos.empty()) {
        const TransactionStatus &first = infos.front();
        const TransactionStatus &last = infos.back();
        const size_t maxBlockNum = std::max(first.blockNumber, last.blockNumber); // хз уже помнит, где находится последняя транзакция, спереди или сзади, так что поверим обе
        return maxBlockNum;
    } else {
        return 0;
    }
}

template<>
bool isGreater<std::vector<TransactionInfo>>(const std::vector<TransactionInfo> &infos, size_t blockNumber) {
    if (infos.empty()) {
        return false;
    }
    return getMaxBlockNumber(infos) > blockNumber;
}

template<>
bool isGreater<std::vector<TransactionStatus>>(const std::vector<TransactionStatus> &infos, size_t blockNumber) {
    if (infos.empty()) {
        return false;
    }
    return getMaxBlockNumber(infos) > blockNumber;
}

template<>
bool isGreater<BalanceInfo>(const BalanceInfo &info, size_t blockNumber) {
    return info.blockNumber > blockNumber;
}

template Address::Address<const char*>(const char*, const char*);
template Address::Address<std::string::const_iterator>(std::string::const_iterator, std::string::const_iterator);

}
