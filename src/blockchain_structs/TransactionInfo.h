#ifndef TORRENT_NODE_TRANSACTIONINFO_H
#define TORRENT_NODE_TRANSACTIONINFO_H

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <set>
#include <unordered_map>

#include "duration.h"

#include "blockchain_structs/Address.h"
#include "blockchain_structs/FilePosition.h"

namespace torrent_node_lib {

struct TransactionStatus {
    TransactionStatus(const std::string &txHash, size_t blockNumber)
        : transaction(txHash), blockNumber(blockNumber) {
    }

    struct Delegate {
        void serialize(std::vector<char> &buffer) const {
        }

        static Delegate deserialize(const std::string &raw, size_t &fromPos) {
            return Delegate();
        }
    };

    struct UnDelegate {
        int64_t value;
        std::string delegateHash;

        UnDelegate() = default;

        UnDelegate(int64_t value, const std::string &delegateHash)
            : value(value), delegateHash(delegateHash) {
        }

        void serialize(std::vector<char> &buffer) const;

        static UnDelegate deserialize(const std::string &raw, size_t &fromPos);
    };

    struct V8Status {
        bool isServerError = false;
        bool isScriptError = false;

        torrent_node_lib::Address compiledContractAddress;

        void serialize(std::vector<char> &buffer) const;

        static V8Status deserialize(const std::string &raw, size_t &fromPos);
    };

    bool isSuccess;

    std::string transaction;

    size_t blockNumber;

    std::variant<std::monostate, Delegate, UnDelegate, V8Status> status;

    void serialize(std::vector<char> &buffer) const;

    static TransactionStatus deserialize(const std::string &raw);

private:

    TransactionStatus() = default;

};

struct TransactionInfo {
public:

    struct DelegateInfo {
        int64_t value;
        bool isDelegate;
    };

    struct ScriptInfo {
        enum class ScriptType {
            compile, run, pay, unknown
        };

        std::string txRaw;
        ScriptType type = ScriptType::unknown;
    };

    struct TokenInfo {
        struct Create {
            std::string type;
            torrent_node_lib::Address owner;
            int decimals;
            size_t value;
            std::string symbol;
            std::string name;
            bool emission;
            std::vector<std::pair<torrent_node_lib::Address, size_t>> beginDistribution;
        };

        struct ChangeOwner {
            torrent_node_lib::Address newOwner;
        };

        struct ChangeEmission {
            bool newEmission;
        };

        struct AddTokens {
            torrent_node_lib::Address toAddress;
            size_t value;
        };

        struct MoveTokens {
            torrent_node_lib::Address toAddress;
            size_t value;
        };

        std::variant<Create, ChangeOwner, ChangeEmission, AddTokens, MoveTokens> info;
    };

public:

    std::string hash;
    torrent_node_lib::Address fromAddress;
    torrent_node_lib::Address toAddress;
    int64_t value;
    int64_t fees = 0;
    uint64_t nonce = 0;
    size_t blockNumber = 0;
    size_t blockIndex = 0;
    size_t sizeRawTx = 0;

    bool isSignBlockTx = false;

    bool isModuleNotSet = false;

    std::optional<uint64_t> intStatus;

    std::vector<char> sign;
    std::vector<unsigned char> pubKey;

    std::vector<unsigned char> data;

    std::string allRawTx;

    torrent_node_lib::FilePosition filePos;

    std::optional<DelegateInfo> delegate;

    std::optional<ScriptInfo> scriptInfo;

    std::optional<TokenInfo> tokenInfo;

    std::optional<TransactionStatus> status;

public:

    void serialize(std::vector<char> &buffer) const;

    static TransactionInfo deserialize(const std::string &raw);

    static TransactionInfo deserialize(const std::string &raw, size_t &from);

    size_t realFee() const;

    bool isStatusNeed() const {
        return delegate.has_value() || scriptInfo.has_value();
    }

    bool isIntStatusNoBalance() const;

    bool isIntStatusNotSuccess() const;

    bool isIntStatusForging() const;

    bool isIntStatusNodeTest() const;

    static std::set<uint64_t> getForgingIntStatuses();
};

} // namespace torrent_node_lib {

#endif //TORRENT_NODE_TRANSACTIONINFO_H
