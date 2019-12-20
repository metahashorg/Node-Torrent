#include "NetworkRejectedBlockSource.h"

#include "check.h"
#include "log.h"

#include "BlockchainRead.h"

using namespace common;

namespace torrent_node_lib {

static const size_t MAXIMUM_REJECTED_BLOCKS = 1000;

NetworkRejectedBlockSource::NetworkRejectedBlockSource(P2P &p2p, bool isCompress)
    : getterBlocks(p2p, isCompress)
{
}

std::pair<std::vector<std::vector<unsigned char>>, std::vector<size_t>> NetworkRejectedBlockSource::getMissingBlocks(const std::vector<RejectedBlockMessage> &headers) const {
    std::vector<std::vector<unsigned char>> missingHashes;
    std::vector<size_t> missingIndices;

    const auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();
    for (size_t i = 0; i < headers.size(); i++) {
        const RejectedBlockMessage &block = headers[i];
        const auto &hash = block.hash;
        const auto found = iterHashFabric.find(hash);
        if (found == iterHashFabric.end()) {
            missingHashes.emplace_back(hash);
            missingIndices.emplace_back(i);
        }
    }

    return std::make_pair(missingHashes, missingIndices);
}

std::vector<NetworkRejectedBlockSource::BlockHolder> NetworkRejectedBlockSource::parseBlocks(const std::vector<RejectedBlockMessage> &headers, const std::vector<std::string> &blockDumps, const std::vector<size_t> &missingIndices) const {
    CHECK(blockDumps.size() == missingIndices.size(), "Incorrect block dumps");

    std::vector<BlockHolder> holders;
    for (size_t i = 0; i < missingIndices.size(); i++) {
        const RejectedBlockMessage &blockHeader = headers.at(missingIndices[i]);
        const std::string &dump = blockDumps[i];
        if (dump.empty()) {
            continue;
        }

        BlockHolder holder;
        holder.dump = dump;
        holder.number = blockHeader.blockNumber;

        holder.info = parseRejectedTxsBlockInfo(dump.data(), dump.data() + dump.size(), 0, true);

        CHECK(holder.info.header.hash == blockHeader.hash && holder.info.header.timestamp == blockHeader.timestamp, "Incorrect parsed block");

        holders.emplace_back(holder);
    }

    return holders;
}

void NetworkRejectedBlockSource::addNewBlocks(const std::vector<BlockHolder> &holders) {
    for (const BlockHolder &holder: holders) {
        blocks.insert(holder);
    }
    auto &iterSequenceFabric = blocks.get<BlockHolder::SequenceTag>();
    if (iterSequenceFabric.size() > MAXIMUM_REJECTED_BLOCKS) {
        const size_t countRemoved = iterSequenceFabric.size() - MAXIMUM_REJECTED_BLOCKS;
        iterSequenceFabric.erase(iterSequenceFabric.begin(), std::next(iterSequenceFabric.begin(), countRemoved));
    }
}

std::vector<RejectedBlockResult> NetworkRejectedBlockSource::getLastBlocks(size_t count) const {
    std::vector<RejectedBlockResult> results;

    const auto &iterSequenceFabric2 = blocks.get<BlockHolder::SequenceTag>();

    std::transform(iterSequenceFabric2.rbegin(), std::next(iterSequenceFabric2.rbegin(), std::min(count, iterSequenceFabric2.size())), std::back_inserter(results), [](const BlockHolder &holder) {
        return RejectedBlockResult(holder.hash(), holder.number, holder.timestamp());
    });

    return results;
}

std::vector<RejectedBlockResult> NetworkRejectedBlockSource::calcLastBlocks(size_t count) {
    const std::vector<RejectedBlockMessage> blockHeaders = getterBlocks.getLastRejectedBlocks(count);

    std::unique_lock<std::mutex> lock(mut);
    const auto [missingHashes, missingIndices] = getMissingBlocks(blockHeaders);
    CHECK(missingHashes.size() == missingIndices.size(), "Incorrect missing blocks");
    lock.unlock();

    const std::vector<std::string> blockDumps = getterBlocks.getRejectedBlocksDumps(missingHashes);

    const std::vector<BlockHolder> holders = parseBlocks(blockHeaders, blockDumps, missingIndices);

    lock.lock();
    addNewBlocks(holders);
    lock.unlock();

    lock.lock();
    return getLastBlocks(count);
}

std::vector<RejectedBlock> NetworkRejectedBlockSource::getBlocks(const std::vector<std::vector<unsigned char>> &hashes) const {
    std::vector<RejectedBlock> result;

    std::lock_guard<std::mutex> lock(mut);

    const auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();

    for (const std::vector<unsigned char> &hash: hashes) {
        const auto iter = iterHashFabric.find(hash);
        if (iter != iterHashFabric.end()) {
            result.emplace_back(iter->info, iter->number);
        }
    }

    return result;
}

std::vector<std::string> NetworkRejectedBlockSource::getDumps(const std::vector<std::vector<unsigned char>> &hashes) const {
    std::vector<std::string> result;

    std::lock_guard<std::mutex> lock(mut);

    const auto &iterHashFabric = blocks.get<BlockHolder::HashTag>();

    for (const std::vector<unsigned char> &hash: hashes) {
        const auto iter = iterHashFabric.find(hash);
        if (iter != iterHashFabric.end()) {
            result.emplace_back(iter->dump);
        }
    }

    return result;
}

} // namespace torrent_node_lib {
