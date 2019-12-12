#ifndef GENERATE_JSON_H_
#define GENERATE_JSON_H_

#include <string>
#include <variant>
#include <functional>

namespace torrent_node_lib {
class BlockChainReadInterface;
struct BlockHeader;
struct SignTransactionInfo;
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

std::string blockHeaderToP2PJson(const RequestId &requestId, const torrent_node_lib::BlockHeader &bh, const std::vector<std::vector<unsigned char>> &prevSignaturesBlocks, const std::vector<std::vector<unsigned char>> &nextSignaturesBlocks, bool isFormat, BlockTypeInfo type, const JsonVersion &version);

std::string blockHeadersToP2PJson(const RequestId &requestId, const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<std::vector<unsigned char>>> &blockSignatures, bool isFormat, const JsonVersion &version);

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version);

std::string genCountBlockForP2PJson(const RequestId &requestId, size_t countBlocks, const std::vector<std::vector<unsigned char>> &signaturesBlocks, bool isFormat, const JsonVersion &version);

std::string preLoadBlocksJson(const RequestId &requestId, size_t countBlocks, const std::vector<torrent_node_lib::BlockHeader> &bh, const std::vector<std::vector<std::vector<unsigned char>>> &blockSignatures, const std::vector<std::string> &blocks, bool isCompress, const JsonVersion &version);

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat);

std::string genDumpBlockBinary(const std::string &block, bool isCompress);

std::string genDumpBlocksBinary(const std::vector<std::string> &blocks, bool isCompress);

#endif // GENERATE_JSON_H_
