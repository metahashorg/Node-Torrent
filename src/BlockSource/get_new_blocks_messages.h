#ifndef GET_NEW_BLOCKS_MESSAGES_H_
#define GET_NEW_BLOCKS_MESSAGES_H_

#include <string>
#include <optional>
#include <vector>
#include <set>

namespace torrent_node_lib {

struct MinimumBlockHeader;
    
struct PreloadBlocksResponse {
    size_t countBlocks = 0;
    std::string blockHeaders;
    std::string blockDumps;
    
    std::optional<std::string> error;
};

std::string makeGetCountBlocksMessage();

std::string makePreloadBlocksMessage(size_t currentBlock, bool isCompress, bool isSign, size_t preloadBlocks, size_t maxBlockSize);

std::string makeGetBlocksMessage(size_t beginBlock, size_t countBlocks);

std::string makeGetBlockByNumberMessage(size_t blockNumber);

std::string makeGetDumpBlockMessage(const std::string &blockHash, size_t fromByte, size_t toByte, bool isSign, bool isCompress);

std::string makeGetDumpBlockMessage(const std::string &blockHash, bool isSign, bool isCompress);

std::string makeGetDumpsBlocksMessage(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end, bool isSign, bool isCompress);

std::pair<size_t, std::set<std::vector<unsigned char>>> parseCountBlocksMessage(const std::string &response);

PreloadBlocksResponse parsePreloadBlocksMessage(const std::string &response);

std::string parseDumpBlockBinary(const std::string &response, bool isCompress);

std::vector<std::string> parseDumpBlocksBinary(const std::string &response, bool isCompress);

torrent_node_lib::MinimumBlockHeader parseBlockHeader(const std::string &response);

std::vector<torrent_node_lib::MinimumBlockHeader> parseBlocksHeader(const std::string &response);

std::optional<std::string> checkErrorGetBlockResponse(const std::string &response, size_t countBlocks);

std::optional<std::string> checkErrorGetBlockDumpResponse(const std::string &response, bool manyBlocks, bool isSign, bool isCompress, size_t sizeDump);

} // namespace torrent_node_lib

#endif // GET_NEW_BLOCKS_MESSAGES_H_
