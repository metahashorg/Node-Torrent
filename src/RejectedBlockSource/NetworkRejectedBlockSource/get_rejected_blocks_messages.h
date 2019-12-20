#ifndef TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H
#define TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H

#include <string>
#include <vector>
#include <optional>

#include "NetworkRejectedBlockSourceStructs.h"

namespace torrent_node_lib {

std::string makeGetLastRejectedBlocksMessage(size_t count);

std::string makeGetRejectedBlocksDumpsMessage(std::vector<std::vector<unsigned char>>::const_iterator hashesBegin, std::vector<std::vector<unsigned char>>::const_iterator hashesEnd, bool isCompress);

std::vector<RejectedBlockMessage> parseGetLastRejectedBlocksMessage(const std::string &message);

std::vector<std::string> parseRejectedDumpBlocksBinary(const std::string &response, bool isCompress);

std::optional<std::string> checkErrorGetRejectedBlockDumpResponse(const std::string &response);

} // namespace torrent_node_lib

#endif //TORRENT_NODE_GET_REJECTED_BLOCKS_MESSAGES_H
