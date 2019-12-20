#ifndef TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCESTRUCTS_H
#define TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCESTRUCTS_H

#include <vector>

namespace torrent_node_lib {

struct RejectedBlockMessage {
    std::vector<unsigned char> hash;
    size_t blockNumber;
    size_t timestamp;
};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCESTRUCTS_H
