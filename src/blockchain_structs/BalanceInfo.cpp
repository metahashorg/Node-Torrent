//
// Created by user on 16.12.2019.
//

#include <blockchain_structs/TransactionInfo.h>
#include <blockchain_structs/Address.h>
#include <check.h>
#include <utils/serialize.h>
#include "BalanceInfo.h"

namespace torrent_node_lib {

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
}