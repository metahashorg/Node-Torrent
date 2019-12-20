#ifndef TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCE_H
#define TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCE_H

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
#include <boost/multi_index/composite_key.hpp>

#include "blockchain_structs/RejectedTxsBlock.h"

#include "RejectedBlockSource/RejectedBlockSource.h"

#include "GetNewRejectedBlocksFromServer.h"

namespace torrent_node_lib {

class P2P;

class NetworkRejectedBlockSource: public RejectedBlockSource {
private:

    struct BlockHolder {
        std::string dump;
        RejectedTxsBlockInfo info;
        size_t number;

        std::vector<unsigned char> hash() const {
            return info.header.hash;
        }

        size_t timestamp() const {
            return info.header.timestamp;
        }

        struct SequenceTag {};

        struct HashTag {};
    };

    using BlocksContainer = boost::multi_index::multi_index_container<
        BlockHolder,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<BlockHolder::SequenceTag>,
                boost::multi_index::composite_key<
                    BlockHolder,
                    boost::multi_index::member<BlockHolder, size_t, &BlockHolder::number>,
                    boost::multi_index::const_mem_fun<BlockHolder, size_t, &BlockHolder::timestamp>
                >
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<BlockHolder::HashTag>,
                boost::multi_index::const_mem_fun<BlockHolder, std::vector<unsigned char>, &BlockHolder::hash>
            >
        >
    >;

public:

    NetworkRejectedBlockSource(P2P &p2p, bool isCompress);

public:

    std::vector<RejectedBlockResult> calcLastBlocks(size_t count) override;

    std::vector<RejectedBlock> getBlocks(const std::vector<std::vector<unsigned char>> &hashes) const override;

    std::vector<std::string> getDumps(const std::vector<std::vector<unsigned char>> &hashes) const override;

private:

    std::pair<std::vector<std::vector<unsigned char>>, std::vector<size_t>> getMissingBlocks(const std::vector<RejectedBlockMessage> &headers) const;

    std::vector<BlockHolder> parseBlocks(const std::vector<RejectedBlockMessage> &headers, const std::vector<std::string> &blockDumps, const std::vector<size_t> &missingIndices) const;

    void addNewBlocks(const std::vector<BlockHolder> &holders);

    std::vector<RejectedBlockResult> getLastBlocks(size_t count) const;

private:

    GetNewRejectedBlocksFromServer getterBlocks;

    BlocksContainer blocks;

    mutable std::mutex mut;
};

} // namespace torrent_node_lib {

#endif //TORRENT_NODE_NETWORKREJECTEDBLOCKSOURCE_H
