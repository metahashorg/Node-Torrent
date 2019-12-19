#ifndef TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H
#define TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H

#include <string>
#include <vector>

namespace torrent_node_lib {

struct RejectedBlockMessage {
    std::vector<unsigned char> hash;
    size_t blockNumber;
    size_t timestamp;
};

std::string makeGetLastRejectedBlocksMessage(size_t count);

std::string makeGetRejectedBlocksDumpsMessage(const std::vector<std::vector<unsigned char>> &hashes);

std::vector<RejectedBlockMessage> parseGetLastRejectedBlocksMessage(const std::string &message);

std::vector<std::string> parseRejectedDumpBlocksBinary(const std::string &response, bool isCompress);

} // namespace torrent_node_lib

#endif //TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H
