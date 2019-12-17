#include "RejectedTxsBlock.h"

namespace torrent_node_lib {

size_t RejectedTxsBlockHeader::endBlockPos() const {
    return filePos.pos + blockSize + sizeof(uint64_t);
}

} // namespace torrent_node_lib {
