#ifndef TORRENT_NODE_BALANCEINFO_H
#define TORRENT_NODE_BALANCEINFO_H

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {

struct Address;
struct TransactionInfo;

struct BalanceInfo {
public:

    class BalanceElement {
    public:
        void receiveValue(size_t value);

        void spentValue(size_t value);

        size_t received() const;

        size_t spent() const;

        size_t balance() const;

        void fill(size_t received, size_t spent);

    private:
        size_t received_ = 0;
        size_t spent_ = 0;
    };

    struct DelegateBalance {
        class DelegateElement {
        public:

            void delegateValue(size_t value) {
                balance.spentValue(value);
            }

            void undelegateValue(size_t value) {
                balance.receiveValue(value);
            }

            size_t delegate() const {
                return balance.spent();
            }

            size_t undelegate() const {
                return balance.received();
            }

            void fill(size_t delegate, size_t undelegate) {
                balance.fill(undelegate, delegate);
            }

        private:

            BalanceElement balance;
        };

        class DelegatedElement {
        public:

            void delegatedValue(size_t value) {
                balance.receiveValue(value);
            }

            void undelegatedValue(size_t value) {
                balance.spentValue(value);
            }

            size_t delegated() const {
                return balance.received();
            }

            size_t undelegated() const {
                return balance.spent();
            }

            void fill(size_t delegated, size_t undelegated) {
                balance.fill(delegated, undelegated);
            }

        private:

            BalanceElement balance;
        };

        DelegateElement delegate;
        DelegatedElement delegated;
        size_t reserved = 0;
        size_t countOp = 0;
    };

    struct ForgedBalance {
        size_t forged = 0;
        size_t countOp = 0;
    };

    struct TokenBalance {
        BalanceElement balance;
        size_t countOp = 0;
        size_t countReceived = 0;
        size_t countSpent = 0;
    };

public:

    BalanceElement balance;
    size_t countReceived = 0;
    size_t countSpent = 0;
    size_t countTxs = 0;

    std::optional<DelegateBalance> delegated;

    std::optional<ForgedBalance> forged;

    std::optional<size_t> hash;

    size_t blockNumber = 0;

    std::optional<size_t> tokenBlockNumber;
    
    std::unordered_map<std::string, TokenBalance> tokens;

public:

    BalanceInfo() = default;

    void plusWithDelegate(const torrent_node_lib::TransactionInfo &tx, const torrent_node_lib::Address &address,
                          const std::optional<int64_t> &undelegateValue, bool isOkStatus);

    void plusWithoutDelegate(const torrent_node_lib::TransactionInfo &tx, const torrent_node_lib::Address &address,
                             bool changeBalance, bool isForging);

    void addTokens(const torrent_node_lib::TransactionInfo &tx, size_t value, bool isOkStatus);

    void moveTokens(const torrent_node_lib::TransactionInfo &tx, const torrent_node_lib::Address &address,
                    const torrent_node_lib::Address &toAddress, size_t value, bool isOkStatus);

    BalanceInfo &operator+=(const BalanceInfo &second);

    size_t received() const;

    size_t spent() const;

    int64_t calcBalance();

    int64_t calcBalanceWithoutDelegate() const;

    void serialize(std::vector<char> &buffer) const;

    static BalanceInfo deserialize(const std::string &raw);

};

BalanceInfo operator+(const BalanceInfo &first, const BalanceInfo &second);

} // namespace torrent_node_lib {

#endif //TORRENT_NODE_BALANCEINFO_H
