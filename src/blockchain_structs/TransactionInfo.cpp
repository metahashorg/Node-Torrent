#include <check.h>
#include <utils/serialize.h>
#include <blockchain_structs/FilePosition.h>
#include "TransactionInfo.h"

namespace torrent_node_lib {

const static uint64_t TX_STATE_APPROVE = 1;
const static uint64_t TX_STATE_ACCEPT = 20;
const static uint64_t TX_STATE_WRONG_MONEY = 30;
const static uint64_t TX_STATE_WRONG_DATA = 40;
const static uint64_t TX_STATE_FORGING = 100;
const static uint64_t TX_STATE_FORGING_W = 101;
const static uint64_t TX_STATE_FORGING_N = 102;
const static uint64_t TX_STATE_FORGING_C = 103;
const static uint64_t TX_STATE_FORGING_A = 104;
const static uint64_t TX_STATE_STATE = 200;
const static uint64_t TX_STATE_TECH_NODE_STAT = 0x1101;

void TransactionInfo::serialize(std::vector<char>& buffer) const {
    CHECK(blockNumber != 0, "block number not setted");

    filePos.serialize(buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeInt<size_t>(blockIndex, buffer);
}

TransactionInfo TransactionInfo::deserialize(const std::string& raw, size_t &from) {
    TransactionInfo result;

    result.filePos = FilePosition::deserialize(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.blockIndex = deserializeInt<size_t>(raw, from);

    return result;
}

TransactionInfo TransactionInfo::deserialize(const std::string& raw) {
    size_t from = 0;
    return deserialize(raw, from);
}

size_t TransactionInfo::realFee() const {
    CHECK(sizeRawTx > 0, "Size raw tx not set");
    return std::min(sizeRawTx > 255 ? sizeRawTx - 255 : 0, (size_t)fees);
}

bool TransactionInfo::isIntStatusNoBalance() const {
    return intStatus.has_value() && intStatus.value() == TX_STATE_WRONG_MONEY;
}

bool TransactionInfo::isIntStatusNotSuccess() const {
    return intStatus.has_value() && (intStatus.value() == TX_STATE_WRONG_MONEY || intStatus.value() == TX_STATE_WRONG_DATA);
}

std::set<uint64_t> TransactionInfo::getForgingIntStatuses() {
    return {TX_STATE_FORGING, TX_STATE_FORGING_C, TX_STATE_FORGING_N, TX_STATE_FORGING_W, TX_STATE_FORGING_A};
}

bool TransactionInfo::isIntStatusForging() const {
    const auto set = getForgingIntStatuses();
    return intStatus.has_value() && set.find(intStatus.value()) != set.end();
}

bool TransactionInfo::isIntStatusNodeTest() const {
    return intStatus.has_value() && intStatus.value() == TX_STATE_TECH_NODE_STAT;
}

void TransactionStatus::UnDelegate::serialize(std::vector<char>& buffer) const {
    serializeInt<uint64_t>(value, buffer);
    serializeString(delegateHash, buffer);
}

TransactionStatus::UnDelegate TransactionStatus::UnDelegate::deserialize(const std::string &raw, size_t &fromPos) {
    UnDelegate result;
    result.value = deserializeInt<uint64_t>(raw, fromPos);
    result.delegateHash = deserializeString(raw, fromPos);
    return result;
}

void TransactionStatus::V8Status::serialize(std::vector<char>& buffer) const {
    serializeInt<uint8_t>(isServerError, buffer);
    serializeInt<uint8_t>(isScriptError, buffer);
    if (!compiledContractAddress.isEmpty()) {
        serializeString(compiledContractAddress.getBinaryString(), buffer);
    } else {
        serializeString("", buffer);
    }
}

TransactionStatus::V8Status TransactionStatus::V8Status::deserialize(const std::string &raw, size_t &fromPos) {
    V8Status result;
    result.isServerError = deserializeInt<uint8_t>(raw, fromPos);
    result.isScriptError = deserializeInt<uint8_t>(raw, fromPos);
    const std::string addressString = deserializeString(raw, fromPos);
    if (!addressString.empty()) {
        result.compiledContractAddress = Address(addressString.begin(), addressString.end());
    } else {
        result.compiledContractAddress.setEmpty();
    }
    return result;
}

void TransactionStatus::serialize(std::vector<char>& buffer) const {
    serializeInt<uint8_t>(isSuccess, buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeString(transaction, buffer);
    CHECK(status.index() != 0, "Status not set");
    serializeVariant(status, buffer);
}

TransactionStatus TransactionStatus::deserialize(const std::string& raw) {
    TransactionStatus result;

    size_t from = 0;
    result.isSuccess = deserializeInt<uint8_t>(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.transaction = deserializeString(raw, from);
    deserializeVarint(raw, from, result.status);
    CHECK(result.status.index() != 0, "Incorrect deserialize");

    return result;
}
}