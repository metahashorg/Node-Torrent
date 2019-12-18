#include "RejectedTxsBlock.h"

#include "check.h"

namespace torrent_node_lib {

void RejectedTxsMinimumBlockHeader::applyFileNameRelative(const std::string &fileNameRelative) {
    filePos.fileNameRelative = fileNameRelative;
}

size_t RejectedTxsMinimumBlockHeader::endBlockPos() const {
    CHECK(blockSize != 0, "Incorrect block size");
    return filePos.pos + blockSize + sizeof(uint64_t);
}

} // namespace torrent_node_lib {
