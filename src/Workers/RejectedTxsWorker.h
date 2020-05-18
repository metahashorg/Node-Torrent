#ifndef TORRENT_NODE_REJECTEDTXSWORKER_H
#define TORRENT_NODE_REJECTEDTXSWORKER_H

#include <mutex>
#include <set>
#include <vector>
#include <map>
#include <optional>

#include "Thread.h"

#include "blockchain_structs/RejectedTxsBlock.h"

#include "duration.h"

namespace torrent_node_lib {

class RejectedBlockSource;

struct RejectedBlock;
struct RejectedBlockResult;

class RejectedTxsWorker {
private:

    struct HistoryElement {
        RejectedTransactionHistory history;
        time_point lastUpdateTime;

        explicit HistoryElement(const RejectedTransactionHistory &history)
            : history(history)
        {}
    };

public:

    RejectedTxsWorker(RejectedBlockSource &blockSource);

    ~RejectedTxsWorker();

    void join();

    void start();

public:

    std::optional<RejectedTransactionHistory> findTx(const std::vector<unsigned char> &txHash) const;

    std::vector<std::string> getDumps(const std::vector<std::vector<unsigned char>> &hashes) const;

    std::vector<RejectedBlockResult> calcLastBlocks(size_t count);

private:

    void worker();

private:

    std::vector<RejectedBlockResult> filterNewBlocks(const std::vector<RejectedBlockResult> &blocks) const;

    void processBlocks(const std::vector<RejectedBlock> &blocks, const std::vector<std::vector<unsigned char>> &allHashes);

    void clearOldHistory();

    void addLastBlocks(const std::vector<RejectedBlockResult> &newBlocks);

private:

    void addHistory(const RejectedTransactionInfo &txInfo, size_t blockNumber, size_t timestamp);

private:

    RejectedBlockSource &blockSource;

    common::Thread workerThread;

    mutable std::mutex mut;

private:

    std::set<std::vector<unsigned char>> currentRejectedBlocks;

    std::map<std::vector<unsigned char>, HistoryElement> history;

    std::multimap<size_t, RejectedBlockResult> lastRejectedBlocks;
};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_REJECTEDTXSWORKER_H
