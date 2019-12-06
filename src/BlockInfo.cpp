#include "BlockInfo.h"

#include "check.h"
#include "utils/serialize.h"
#include "convertStrings.h"
#include "stringUtils.h"
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

const static uint64_t TX_STATE_APPROVE = 1; //block approve transaction
const static uint64_t TX_STATE_ACCEPT = 20; // transaction accepted (data & move)
const static uint64_t TX_STATE_WRONG_MONEY = 30; // transaction not accepted (insuficent funds)
const static uint64_t TX_STATE_WRONG_DATA = 40; // transaction not accepted (data method rejected)
const static uint64_t TX_STATE_FORGING = 100; // forging transaction
const static uint64_t TX_STATE_FORGING_W = 101; // wallet forging transaction
const static uint64_t TX_STATE_FORGING_N = 102; // node forging transaction
const static uint64_t TX_STATE_FORGING_C = 103; // coin forging transaction
const static uint64_t TX_STATE_FORGING_A = 104; // ???
const static uint64_t TX_STATE_STATE = 200; // state block transaction
const static uint64_t TX_STATE_TECH_NODE_STAT = 0x1101;

std::string FilePosition::serialize() const {
    CHECK(!fileNameRelative.empty(), "FilePosition not initialized");
    
    std::string res;
    res.reserve(50);
    res += serializeString(fileNameRelative);
    res += serializeInt<size_t>(pos);
    return res;
}

void FilePosition::serialize(std::vector<char>& buffer) const {
    CHECK(!fileNameRelative.empty(), "FilePosition not initialized");
    serializeString(fileNameRelative, buffer);
    serializeInt<size_t>(pos, buffer);
}

FilePosition FilePosition::deserialize(const std::string& raw, size_t from, size_t &nextFrom) {
    FilePosition result;
    
    result.fileNameRelative = deserializeString(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;
    
    result.pos = deserializeInt<size_t>(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;
    
    return result;
}

FilePosition FilePosition::deserialize(const std::string& raw, size_t& from) {
    size_t endPos = from;
    const FilePosition res = FilePosition::deserialize(raw, from, endPos);
    CHECK(endPos != from, "Incorrect raw");
    from = endPos;
    return res;
}

FilePosition FilePosition::deserialize(const std::string& raw) {
    size_t tmp;
    return deserialize(raw, 0, tmp);
}

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
    const auto set = TransactionInfo::getForgingIntStatuses();
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
    TransactionStatus::UnDelegate result;
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
    TransactionStatus::V8Status result;
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

void AddressInfo::serialize(std::vector<char>& buffer) const {
    CHECK(blockNumber != 0, "AddressInfo not initialized");
    
    filePos.serialize(buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeInt<size_t>(blockIndex, buffer);
    if (undelegateValue.has_value()) {
        serializeInt<uint64_t>(undelegateValue.value(), buffer);
    }
}

AddressInfo AddressInfo::deserialize(const std::string& raw) {
    AddressInfo result;
    
    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.blockIndex = deserializeInt<size_t>(raw, from);
    if (from < raw.size()) {
        result.undelegateValue = deserializeInt<uint64_t>(raw, from);
    }
       
    return result;
}

size_t BalanceInfo::BalanceElement::balance() const {
    return received_ - spent_;
}

void BalanceInfo::BalanceElement::receiveValue(size_t value) {
    if (received_ >= std::numeric_limits<size_t>::max() - value) {
        received_ = balance();
        spent_ = 0;
    }
    received_ += value;
}

void BalanceInfo::BalanceElement::spentValue(size_t value) {
    if (spent_ >= std::numeric_limits<size_t>::max() - value) {
        received_ = balance();
        spent_ = 0;
    }
    spent_ += value;
}

size_t BalanceInfo::BalanceElement::received() const {
    return received_;
}

size_t BalanceInfo::BalanceElement::spent() const {
    return spent_;
}

void BalanceInfo::BalanceElement::fill(size_t received, size_t spent) {
    received_ = received;
    spent_ = spent;
}

void BalanceInfo::plusWithDelegate(const TransactionInfo &tx, const Address &address, const std::optional<int64_t> &undelegateValue, bool isOkStatus) {
    CHECK(tx.delegate.has_value(), "Tx not delegate");
    DelegateBalance delegateBalance;
    if (delegated.has_value()) {
        delegateBalance = delegated.value();
    }
    delegateBalance.countOp++;
    
    if (isOkStatus) {
        if (tx.fromAddress == address) {
            if (tx.delegate->isDelegate) {
                balance.spentValue(tx.delegate->value);
                delegateBalance.delegate.delegateValue(tx.delegate->value);
            } else {
                CHECK(undelegateValue.has_value(), "Undelegate value not set");
                balance.receiveValue(undelegateValue.value());
                delegateBalance.delegate.undelegateValue(undelegateValue.value());
            }
        }
        
        if (tx.toAddress == address) {
            if (tx.delegate->isDelegate) {
                delegateBalance.delegated.delegatedValue(tx.delegate->value);
            } else {
                CHECK(undelegateValue.has_value(), "Undelegate value not set");
                delegateBalance.delegated.undelegatedValue(undelegateValue.value());
            }
        }
    }
    if (tx.fromAddress == address && tx.delegate->isDelegate) {
        delegateBalance.reserved -= tx.delegate->value;
    }
    
    delegated = delegateBalance;
}

void BalanceInfo::plusWithoutDelegate(const TransactionInfo& tx, const Address& address, bool changeBalance, bool isForging) {
    if (tx.fromAddress == address) {
        countSpent++;
        if (changeBalance) {
            balance.spentValue(tx.value + tx.realFee());
        }
        blockNumber = std::max(blockNumber, tx.blockNumber);
    }
    if (tx.toAddress == address) {
        countReceived++;
        if (changeBalance) {
            balance.receiveValue(tx.value);
        }
        blockNumber = std::max(blockNumber, tx.blockNumber);
    }
    
    countTxs++;
    
    if (tx.delegate.has_value()) {
        DelegateBalance delegateBalance;
        if (delegated.has_value()) {
            delegateBalance = delegated.value();
        }
        if (tx.delegate->isDelegate && tx.fromAddress == address) {
            delegateBalance.reserved += tx.delegate->value;
        }
        delegated = delegateBalance;
    }
    
    if (isForging) {
        if (tx.toAddress == address) {
            ForgedBalance forgedBalance;
            if (forged.has_value()) {
                forgedBalance = forged.value();
            }
            forgedBalance.countOp++;
            forgedBalance.forged += tx.value;
            forged = forgedBalance;
        }
    }
}

void BalanceInfo::addTokens(const TransactionInfo &tx, size_t value, bool isOkStatus) {
    if (!isOkStatus) {
        return;
    }
    if (tx.tokenInfo.has_value()) {
        const Address &tokenAddress = tx.toAddress;
        tokens[tokenAddress.getBinaryString()].countOp++;
        tokens[tokenAddress.getBinaryString()].balance.receiveValue(value);
    }
}

void BalanceInfo::moveTokens(const TransactionInfo &tx, const Address &address, const Address &toAddress, size_t value, bool isOkStatus) {
    if (!isOkStatus) {
        return;
    }
    if (tx.tokenInfo.has_value()) {
        const Address &tokenAddress = tx.toAddress;
        if (tx.fromAddress == address) {
            tokens[tokenAddress.getBinaryString()].countOp++;
            tokens[tokenAddress.getBinaryString()].balance.spentValue(value);
        }
        if (toAddress == address) {
            tokens[tokenAddress.getBinaryString()].countOp++;
            tokens[tokenAddress.getBinaryString()].balance.receiveValue(value);
        }
    }
}

BalanceInfo& BalanceInfo::operator+=(const BalanceInfo& second) {
    balance.receiveValue(second.balance.received());
    countReceived += second.countReceived;
    balance.spentValue(second.balance.spent());
    countSpent += second.countSpent;
    countTxs += second.countTxs;
    
    if (second.delegated.has_value()) {
        DelegateBalance delegateBalance;
        if (delegated.has_value()) {
            delegateBalance = delegated.value();
        }
        
        delegateBalance.countOp += second.delegated->countOp;
        delegateBalance.delegate.delegateValue(second.delegated->delegate.delegate());
        delegateBalance.delegate.undelegateValue(second.delegated->delegate.undelegate());
        delegateBalance.delegated.delegatedValue(second.delegated->delegated.delegated());
        delegateBalance.delegated.undelegatedValue(second.delegated->delegated.undelegated());
        delegateBalance.reserved += second.delegated->reserved;
        
        delegated = delegateBalance;
    }
    
    if (second.forged.has_value()) {
        ForgedBalance forgedBalance;
        if (forged.has_value()) {
            forgedBalance = forged.value();
        }
        forgedBalance.countOp += second.forged->countOp;
        forgedBalance.forged += second.forged->forged;
        
        forged = forgedBalance;
    }
    
    for (const auto &[token, balance]: second.tokens) {
        tokens[token].balance.receiveValue(balance.balance.received());
        tokens[token].balance.spentValue(balance.balance.spent());
        tokens[token].countOp += balance.countOp;
    }
    
    blockNumber = std::max(blockNumber, second.blockNumber);
    return *this;
}

size_t BalanceInfo::received() const {
    return balance.received();
}

size_t BalanceInfo::spent() const {
    return balance.spent();
}

int64_t BalanceInfo::calcBalance() {
    return balance.received() - balance.spent();
}

int64_t BalanceInfo::calcBalanceWithoutDelegate() const {
    return balance.received() - balance.spent() + (delegated.has_value() ? (delegated->delegate.delegate() - delegated->delegate.undelegate()) : 0);
}

BalanceInfo operator+(const BalanceInfo &first, const BalanceInfo &second) {
    BalanceInfo result(first);
    result += second;
    return result;
}

void BalanceInfo::serialize(std::vector<char>& buffer) const {
    serializeInt<size_t>(balance.received(), buffer);
    serializeInt<size_t>(balance.spent(), buffer);
    serializeInt<size_t>(countReceived, buffer);
    serializeInt<size_t>(countSpent, buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeInt<size_t>(countTxs, buffer);
    
    const size_t tokensSize = tokens.size();
    serializeInt(tokensSize, buffer);
    for (const auto &[address, balance]: tokens) {
        serializeString(address, buffer);
        serializeInt(balance.balance.received(), buffer);
        serializeInt(balance.balance.spent(), buffer);
        serializeInt(balance.countOp, buffer);
    }
    
    if (delegated.has_value()) {
        serializeInt<unsigned char>('d', buffer);
        serializeInt<size_t>(delegated->countOp, buffer);
        serializeInt<size_t>(delegated->delegate.delegate(), buffer);
        serializeInt<size_t>(delegated->delegate.undelegate(), buffer);
        serializeInt<size_t>(delegated->delegated.delegated(), buffer);
        serializeInt<size_t>(delegated->delegated.undelegated(), buffer);
        serializeInt<size_t>(delegated->reserved, buffer);
    }
    
    if (forged.has_value()) {
        serializeInt<unsigned char>('f', buffer);
        serializeInt<size_t>(forged->countOp, buffer);
        serializeInt<size_t>(forged->forged, buffer);
    }
}

BalanceInfo BalanceInfo::deserialize(const std::string& raw) {
    BalanceInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    const size_t received = deserializeInt<size_t>(raw, from);
    const size_t spent = deserializeInt<size_t>(raw, from);
    result.balance.fill(received, spent);
    result.countReceived = deserializeInt<size_t>(raw, from);
    result.countSpent = deserializeInt<size_t>(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countTxs = deserializeInt<size_t>(raw, from);
        
    const size_t countTokens = deserializeInt<size_t>(raw, from);
    
    for (size_t i = 0; i < countTokens; i++) {
        const std::string token = deserializeString(raw, from);
        const size_t received = deserializeInt<size_t>(raw, from);
        const size_t spent = deserializeInt<size_t>(raw, from);
        TokenBalance balance;
        balance.balance.fill(received, spent);
        balance.countOp = deserializeInt<size_t>(raw, from);
        
        result.tokens.emplace(token, balance);
    }
    
    while (raw.size() > from) {
        const unsigned char nextType = deserializeInt<unsigned char>(raw, from);
        if (nextType == 'd') {
            DelegateBalance db;
            db.countOp = deserializeInt<size_t>(raw, from);
            const size_t delegate = deserializeInt<size_t>(raw, from);
            const size_t undelegate = deserializeInt<size_t>(raw, from);
            db.delegate.fill(delegate, undelegate);
            const size_t delegated = deserializeInt<size_t>(raw, from);
            const size_t undelegated = deserializeInt<size_t>(raw, from);
            db.delegated.fill(delegated, undelegated);
            db.reserved = deserializeInt<size_t>(raw, from);
            result.delegated = db;
        } else if (nextType == 'f') {
            ForgedBalance forged;
            forged.countOp = deserializeInt<size_t>(raw, from);
            forged.forged = deserializeInt<size_t>(raw, from);
            result.forged = forged;
        } else {
            throwErr("Incorrect type balance " + std::string(1, (char)nextType));
        }
    }
    
    return result;
}

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
