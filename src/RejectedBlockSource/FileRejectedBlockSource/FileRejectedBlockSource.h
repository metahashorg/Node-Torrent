#ifndef TORRENT_NODE_FILEREJECTEDBLOCKSOURCE_H
#define TORRENT_NODE_FILEREJECTEDBLOCKSOURCE_H

#include <vector>
#include <map>
#include <string>
#include <optional>
#include <deque>
#include <mutex>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include "blockchain_structs/RejectedTxsBlock.h"

#include "RejectedBlockSource/RejectedBlockSource.h"

namespace torrent_node_lib {

class BlockChain;

class FileRejectedBlockSource: public RejectedBlockSource {
private:

    struct BlockHolder {
        struct Block {
            std::string dump;
            RejectedTxsBlockInfo info;
            size_t number;

            Block(const std::string &dump, const RejectedTxsBlockInfo &info, size_t number)
                : dump(dump)
                , info(info)
                , number(number)
            {}
        };

        explicit BlockHolder(const RejectedTxsMinimumBlockHeader &minimumHeader, size_t index)
            : minimumHeader(minimumHeader)
            , index(index)
        {}

        RejectedTxsMinimumBlockHeader minimumHeader;

        size_t index;

        std::optional<Block> block;

        std::optional<std::vector<unsigned char>> hash() const {
            if (!block.has_value()) {
                return std::nullopt;
            } else {
                return block->info.header.hash;
            }
        }

        struct SequenceTag{};

        struct IndexTag{};

        struct HashTag{
            HashTag(const Block &block)
                : block(block)
            {}

            void operator()(BlockHolder &holder) {
                holder.block = block;
            }

            Block block;
        };

    };

    // TODO Поменять ordered_non_unique на hashed_non_unique
    using BlocksContainer = boost::multi_index::multi_index_container<
        BlockHolder,
        boost::multi_index::indexed_by<
            boost::multi_index::random_access<boost::multi_index::tag<BlockHolder::SequenceTag>>,
            boost::multi_index::hashed_unique<boost::multi_index::tag<BlockHolder::IndexTag>, boost::multi_index::member<BlockHolder, size_t, &BlockHolder::index>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::tag<BlockHolder::HashTag>, boost::multi_index::const_mem_fun<BlockHolder, std::optional<std::vector<unsigned char>>, &BlockHolder::hash>>
        >
    >;

public:

    FileRejectedBlockSource(const BlockChain &blockchain, std::string folderPath)
        : blockchain(blockchain)
        , folderPath(folderPath)
    {}

public:

    void addBlock(const RejectedTxsMinimumBlockHeader &minimumHeader);

public:

    std::vector<RejectedBlockResult> calcLastBlocks(size_t count) override;

    std::vector<RejectedBlock> getBlocks(const std::vector<std::vector<unsigned char>> &hashes) const override;

    std::vector<std::string> getDumps(const std::vector<std::vector<unsigned char>> &hashes) const override;

private:

    std::vector<BlockHolder> getLastHolders(size_t count) const;

    void fillHolders(std::vector<BlockHolder> &holders);

    void replaceBlocks(const std::vector<BlockHolder> &holders);

    std::vector<RejectedBlockResult> getLastBlocks(const std::vector<BlockHolder> &holders) const;

private:

    const BlockChain &blockchain;

    const std::string folderPath;

    BlocksContainer blocks;

    size_t currIndex = 0;

    mutable std::mutex mut;

};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_FILEREJECTEDBLOCKSOURCE_H
