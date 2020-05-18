#include "RejectedTxsWorker.h"

#include "log.h"
#include "check.h"
#include "stopProgram.h"

#include "RejectedBlockSource/RejectedBlockSource.h"

using namespace common;

namespace torrent_node_lib {

static const auto LAST_TIME_LIFE_RECORD = 15min;

static const size_t MAX_HISTORY_SIZE = 10;

RejectedTxsWorker::RejectedTxsWorker(torrent_node_lib::RejectedBlockSource &blockSource)
    : blockSource(blockSource)
{}

RejectedTxsWorker::~RejectedTxsWorker() {
    try {
        workerThread.join();
    } catch (...) {
        LOGERR << "Error while stoped thread";
    }
}

void RejectedTxsWorker::join() {
    workerThread.join();
}

void RejectedTxsWorker::start() {
    workerThread = Thread(&RejectedTxsWorker::worker, this);
}

std::vector<RejectedBlockResult> RejectedTxsWorker::filterNewBlocks(const std::vector<RejectedBlockResult> &blocks) const {
    std::lock_guard<std::mutex> lock(mut);

    std::vector<RejectedBlockResult> result;
    std::copy_if(blocks.begin(), blocks.end(), std::back_inserter(result), [this](const RejectedBlockResult &block) {
        return currentRejectedBlocks.find(block.hash) == currentRejectedBlocks.end();
    });

    return result;
}

void RejectedTxsWorker::processBlocks(const std::vector<RejectedBlock> &blocks, const std::vector<std::vector<unsigned char>> &allHashes) {
    std::lock_guard<std::mutex> lock(mut);

    for (const RejectedBlock &block: blocks) {
        for (const RejectedTransactionInfo &tx: block.block.txs) {
            addHistory(tx, block.blockNumber, block.block.header.timestamp);
        }
    }

    currentRejectedBlocks = std::set<std::vector<unsigned char>>(allHashes.begin(), allHashes.end());
}

void RejectedTxsWorker::clearOldHistory() {
    const auto now = ::now();

    std::lock_guard<std::mutex> lock(mut);

    for (auto iter = history.begin(); iter != history.end();) {
        if (now - iter->second.lastUpdateTime >= 15min) {
            iter = history.erase(iter);
        } else {
            iter++;
        }
    }
}

void RejectedTxsWorker::addHistory(const torrent_node_lib::RejectedTransactionInfo &txInfo, size_t blockNumber, size_t timestamp) {
    const auto &hash = txInfo.hash;
    if (history.find(hash) == history.end()) {
        history.emplace(hash, HistoryElement(RejectedTransactionHistory(hash)));
    }
    HistoryElement &element = history.at(hash);
    element.lastUpdateTime = ::now();
    element.history.history.emplace_back(blockNumber, timestamp, txInfo.error);

    if (element.history.history.size() > MAX_HISTORY_SIZE) {
        element.history.history.pop_front();
    }
}

void RejectedTxsWorker::addLastBlocks(const std::vector<RejectedBlockResult> &newBlocks) {
    std::lock_guard<std::mutex> lock(mut);
    std::transform(newBlocks.begin(), newBlocks.end(), std::inserter(lastRejectedBlocks, lastRejectedBlocks.begin()), [](const RejectedBlockResult &block) {
        return std::make_pair(block.blockNumber, block);
    });
    const size_t maxCountElems = 500;
    const size_t eraseElements = lastRejectedBlocks.size() > maxCountElems ? lastRejectedBlocks.size() - maxCountElems : 0;
    lastRejectedBlocks.erase(lastRejectedBlocks.begin(), std::next(lastRejectedBlocks.begin(), eraseElements));
}

void RejectedTxsWorker::worker() {
    try {
        while (true) {
            const std::vector<RejectedBlockResult> lastBlocks = blockSource.calcLastBlocks(100);
            const std::vector<RejectedBlockResult> newLastBlocks = filterNewBlocks(lastBlocks);

            std::vector<std::vector<unsigned char>> hashes;
            hashes.reserve(lastBlocks.size());
            std::transform(lastBlocks.begin(), lastBlocks.end(), std::back_inserter(hashes), std::mem_fn(&RejectedBlockResult::hash));

            std::vector<std::vector<unsigned char>> newHashes;
            newHashes.reserve(newLastBlocks.size());
            std::transform(newLastBlocks.begin(), newLastBlocks.end(), std::back_inserter(newHashes), std::mem_fn(&RejectedBlockResult::hash));

            const std::vector<RejectedBlock> blocks = blockSource.getBlocks(newHashes);
            addLastBlocks(newLastBlocks);

            processBlocks(blocks, hashes);

            clearOldHistory();

            if (!blocks.empty()) {
                LOGINFO << "Rejected txs block processed, count blocks " << blocks.size() << ". Number "
                        << blocks[0].blockNumber << ". Count txs " << blocks[0].block.txs.size();
            }

            sleep(1s);
        }
    } catch (const StopException &e) {
        LOGINFO << "Stop synchronize thread";
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::exception &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Unknown error";
    }
}

std::optional<RejectedTransactionHistory> RejectedTxsWorker::findTx(const std::vector<unsigned char> &txHash) const {
    std::lock_guard<std::mutex> lock(mut);

    const auto found = history.find(txHash);
    if (found == history.end()) {
        return std::nullopt;
    } else {
        return found->second.history;
    }
}

std::vector<std::string> RejectedTxsWorker::getDumps(const std::vector<std::vector<unsigned char> > &hashes) const {
    return blockSource.getDumps(hashes);
}

std::vector<RejectedBlockResult> RejectedTxsWorker::calcLastBlocks(size_t count) {
    std::lock_guard<std::mutex> lock(mut);

    const size_t countElements = std::min(lastRejectedBlocks.size(), count);
    std::vector<RejectedBlockResult> blocks;
    blocks.reserve(countElements);
    std::transform(lastRejectedBlocks.rbegin(), std::next(lastRejectedBlocks.rbegin(), countElements), std::back_inserter(blocks), [](const auto &pair) {
        return pair.second;
    });

    return blocks;
}

} // namespace torrent_node_lib
