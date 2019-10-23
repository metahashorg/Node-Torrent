#ifndef GENERATE_JSON_H_
#define GENERATE_JSON_H_

#include <string>
#include <variant>
#include <functional>

namespace torrent_node_lib {
class BlockChainReadInterface;
class Address;
struct TransactionInfo;
struct BlockHeader;
struct BlockInfo;
struct MinimumBlockHeader;
struct BalanceInfo;
struct DelegateState;
struct V8Details;
struct CommonBalance;
struct V8Code;
struct ForgingSums;
struct NodeTestResult;
struct NodeTestTrust;
struct NodeTestCount;
struct NodeTestExtendedStat;
struct Token;
}

struct RequestId {
    std::variant<std::string, size_t> id;
    bool isSet = false;
};

enum class JsonVersion {
    V1, V2
};

enum class BlockTypeInfo {
    ForP2P, Small, Simple, Hashes, Full
};

std::string genErrorResponse(const RequestId &requestId, int code, const std::string &error);

std::string genStatusResponse(const RequestId &requestId, const std::string &version, const std::string &gitHash);

std::string genStatisticResponse(const RequestId &requestId, size_t statistic, double proc, unsigned long long int memory, int connections);

std::string genStatisticResponse(size_t statistic);

std::string genInfoResponse(const RequestId &requestId, const std::string &version, const std::string &privkey);

std::string genTransactionNotFoundResponse(const RequestId &requestId, const std::string &transaction);

std::string transactionToJson(const RequestId &requestId, const torrent_node_lib::TransactionInfo &info, const torrent_node_lib::BlockChainReadInterface &blockchain, size_t countBlocks, size_t knwonBlock, bool isFormat, const JsonVersion &version);

std::string tokenToJson(const RequestId &requestId, const torrent_node_lib::Token &info, bool isFormat, const JsonVersion &version);

std::string transactionsToJson(const RequestId &requestId, const std::vector<torrent_node_lib::TransactionInfo> &infos, const torrent_node_lib::BlockChainReadInterface &blockchain, bool isFormat, const JsonVersion &version);

std::string addressesInfoToJson(const RequestId &requestId, const std::string &address, const std::vector<torrent_node_lib::TransactionInfo> &infos, const torrent_node_lib::BlockChainReadInterface &blockchain, size_t currentBlock, bool isFormat, const JsonVersion &version);

std::string addressesInfoToJsonFilter(const RequestId &requestId, const std::string &address, const std::vector<torrent_node_lib::TransactionInfo> &infos, size_t nextFrom, const torrent_node_lib::BlockChainReadInterface &blockchain, size_t currentBlock, bool isFormat, const JsonVersion &version);

std::string balanceInfoToJson(const RequestId &requestId, const std::string &address, const torrent_node_lib::BalanceInfo &balance, size_t currentBlock, bool isFormat, const JsonVersion &version);

std::string balancesInfoToJson(const RequestId &requestId, const std::vector<std::pair<std::string, torrent_node_lib::BalanceInfo>> &balances, size_t currentBlock, bool isFormat, const JsonVersion &version);

std::string blockHeaderToJson(const RequestId &requestId, const torrent_node_lib::BlockHeader &bh, const std::vector<torrent_node_lib::TransactionInfo> &signatures, bool isFormat, BlockTypeInfo type, const JsonVersion &version);

std::string blockHeadersToJson(const RequestId &requestId, const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<torrent_node_lib::TransactionInfo>> &signatures, BlockTypeInfo type, bool isFormat, const JsonVersion &version);

std::string blockInfoToJson(const RequestId &requestId, const torrent_node_lib::BlockInfo &bi, const std::vector<torrent_node_lib::TransactionInfo> &signatures, BlockTypeInfo type, bool isFormat, const JsonVersion &version);

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version);

std::string preLoadBlocksJson(size_t countBlocks);

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat);

std::string delegateStatesToJson(const RequestId &requestId, const std::string &address, const std::vector<std::pair<torrent_node_lib::Address, torrent_node_lib::DelegateState>> &states, bool isFormat, const JsonVersion &version);

std::string genV8DetailsJson(const RequestId &requestId, const std::string &address, const torrent_node_lib::V8Details &details, const std::string &path, bool isFormat);

std::string genCommonBalanceJson(const RequestId &requestId, const torrent_node_lib::CommonBalance &balance, bool isFormat, const JsonVersion &version);

std::string genV8CodeJson(const RequestId &requestId, const std::string &address, const torrent_node_lib::V8Code &code, bool isFormat);

std::string genForgingSumJson(const RequestId &requestId, const torrent_node_lib::ForgingSums &sums, bool isFormat, const JsonVersion &version);

std::string genNodeStatResultJson(const RequestId &requestId, const std::string &address, size_t lastBlockTimestamp, const torrent_node_lib::NodeTestResult &result, bool isFormat, const JsonVersion &version);

std::string genNodeStatTrustJson(const RequestId &requestId, const std::string &address, size_t lastBlockTimestamp, const torrent_node_lib::NodeTestTrust &result, bool isFormat, const JsonVersion &version);

std::string genNodeStatCountJson(const RequestId &requestId, const std::string &address, size_t lastBlockDay, const torrent_node_lib::NodeTestCount &result, bool isFormat, const JsonVersion &version);

std::string genNodesStatsCountJson(const RequestId &requestId, size_t lastBlockDay, const torrent_node_lib::NodeTestCount &result, bool isFormat, const JsonVersion &version);

std::string genAllNodesStatsCountJson(const RequestId &requestId, size_t lastBlockDay, const std::vector<std::pair<std::string, torrent_node_lib::NodeTestExtendedStat>> &result, bool isFormat, const JsonVersion &version);

std::string genNodesRaitingJson(const RequestId &requestId, const std::string &address, int raiting, size_t day, size_t lastBlockDay, bool isFormat, const JsonVersion &version);

std::string genTestSignStringJson(const RequestId &requestId, const std::string &responseHex);

std::string genDumpBlockBinary(const std::string &block, bool isCompress);

std::string genDumpBlocksBinary(const std::vector<std::string> &blocks, bool isCompress);

std::string parseDumpBlockBinary(const std::string &response, bool isCompress);

std::vector<std::string> parseDumpBlocksBinary(const std::string &response, bool isCompress);

torrent_node_lib::MinimumBlockHeader parseBlockHeader(const std::string &response);

std::vector<torrent_node_lib::MinimumBlockHeader> parseBlocksHeader(const std::string &response);

#endif // GENERATE_JSON_H_
