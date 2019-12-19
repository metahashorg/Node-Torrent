#include "FileRejectedBlockSource.h"

#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "utils/FileSystem.h"

#include "utils/IfStream.h"
#include "BlockchainRead.h"

#include "BlockChain.h"

using namespace common;

namespace torrent_node_lib {

const static size_t MAXIMUM_BLOCKS = 1000;

void FileRejectedBlockSource::addBlock(const torrent_node_lib::RejectedTxsMinimumBlockHeader &minimumHeader) {
    std::lock_guard<std::mutex> lock(mut);

    blocks.emplace_back(minimumHeader, ++currIndex);
    while (blocks.size() > MAXIMUM_BLOCKS) {
        blocks.pop_front();
    }
}

std::vector<RejectedBlockResult> FileRejectedBlockSource::calcLastBlocks(size_t count) {
    CHECK(count <= MAXIMUM_BLOCKS, "Incorrect count");

    std::vector<BlockHolder> holders;

    std::unique_lock<std::mutex> lock(mut);

    const auto &iterSequenceFabric = blocks.get<BlockHolder::SequenceTag>();

    std::copy_n(iterSequenceFabric.rbegin(), std::min(count, iterSequenceFabric.size()), std::back_inserter(holders));

    lock.unlock();

    IfStream file;
    for (BlockHolder &holder: holders) {
        if (holder.block.has_value()) {
            continue;
        }

        file.reopen(getFullPath(holder.minimumHeader.filePos.fileNameRelative, folderPath));

        std::string dump;
        const size_t newPos = readNextBlockDump(file, holder.minimumHeader.filePos.pos, dump);
        CHECK(newPos != holder.minimumHeader.filePos.pos, "Incorrect rejected block");
        const RejectedTxsBlockInfo blockInfo = parseRejectedTxsBlockInfo(dump.data(), dump.data() + dump.size(), holder.minimumHeader.filePos.pos, true);

        const BlockHeader header = blockchain.getBlock(blockInfo.header.prevHash);
        CHECK(header.blockNumber.has_value(), "Block not found in blockchain");

        holder.block = BlockHolder::Block(dump, blockInfo, header.blockNumber.value());
    }

    lock.lock();

    for (const BlockHolder &block: holders) {
        const auto &iterIndexFabric = blocks.get<BlockHolder::IndexTag>();
        auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();
        const auto iter = iterIndexFabric.find(block.index);
        if (iter != iterIndexFabric.end()) {
            CHECK(block.block.has_value(), "Not found block");

            if (!iter->block.has_value()) {
                const auto iter2 = blocks.project<BlockHolder::HashTag>(iter);
                iterHashFabric.modify(iter2, BlockHolder::HashTag(block.block.value()));
            }
        }
    }

    lock.unlock();

    lock.lock();

    std::vector<RejectedBlockResult> result;
    result.reserve(holders.size());
    std::transform(holders.begin(), holders.end(), std::back_inserter(result), [](const BlockHolder &holder) {
        return RejectedBlockResult(holder.block->info.header.hash, holder.block->number, holder.block->info.header.timestamp);
    });
    return result;
}

std::vector<RejectedBlock> FileRejectedBlockSource::getBlocks(const std::vector<std::vector<unsigned char>> &hashes) const {
    std::vector<RejectedBlock> result;

    std::lock_guard<std::mutex> lock(mut);

    const auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();

    for (const std::vector<unsigned char> &hash: hashes) {
        const auto iter = iterHashFabric.find(hash);
        if (iter != iterHashFabric.end()) {
            CHECK(iter->block.has_value(), "Block not filled");
            result.emplace_back(iter->block->info, iter->block->number);
        }
    }

    return result;
}

std::vector<std::string> FileRejectedBlockSource::getDumps(const std::vector<std::vector<unsigned char>> &hashes) const {
    std::vector<std::string> result;

    std::lock_guard<std::mutex> lock(mut);

    const auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();

    for (const std::vector<unsigned char> &hash: hashes) {
        const auto iter = iterHashFabric.find(hash);
        if (iter != iterHashFabric.end()) {
            CHECK(iter->block.has_value(), "Block not filled");
            result.emplace_back(iter->block->dump);
        }
    }

    return result;
}

} // namespace torrent_node_lib
