#ifndef BLOCK_SOURCE_H_
#define BLOCK_SOURCE_H_

#include <string>
#include <variant>

namespace torrent_node_lib {

struct BlockInfo;
struct SignBlockInfo;
struct RejectedTxsBlockInfo;
struct BlockHeader;

class BlockSource {
public:
    
    virtual void initialize() = 0;
    
    virtual std::pair<bool, size_t> doProcess(size_t countBlocks) = 0;
    
    virtual bool process(std::variant<std::monostate, BlockInfo, SignBlockInfo, RejectedTxsBlockInfo> &bi, std::string &binaryDump) = 0;
    
    virtual void getExistingBlock(const BlockHeader &bh, BlockInfo &bi, std::string &blockDump) const = 0;
    
    virtual ~BlockSource() = default;
    
};

}

#endif // BLOCK_SOURCE_H_
