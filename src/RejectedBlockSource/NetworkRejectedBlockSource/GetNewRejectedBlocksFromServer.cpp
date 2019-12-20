#include "GetNewRejectedBlocksFromServer.h"

#include <functional>
using namespace std::placeholders;

#include "check.h"
#include "log.h"

#include "get_rejected_blocks_messages.h"

using namespace common;

namespace torrent_node_lib {

static const size_t COUNT_BLOCKS_IN_BATCH = 100;

std::vector<RejectedBlockMessage> GetNewRejectedBlocksFromServer::getLastRejectedBlocks(size_t countLast) {
    std::vector<RejectedBlockMessage> bestBlocks;
    size_t maxTimestamp = 0;
    std::string error;
    std::mutex mut;
    const BroadcastResult function = [&bestBlocks, &maxTimestamp, &error, &mut](const std::string &server, const std::string &result, const std::optional<CurlException> &curlException) {
        if (curlException.has_value()) {
            std::lock_guard<std::mutex> lock(mut);
            error = curlException.value().message;
            return;
        }

        try {
            const std::vector<RejectedBlockMessage> blocks = parseGetLastRejectedBlocksMessage(result);
            const auto maxElement = std::max_element(blocks.begin(), blocks.end(), [](const RejectedBlockMessage &first, const RejectedBlockMessage &second) {
                return first.timestamp < second.timestamp;
            });

            std::lock_guard<std::mutex> lock(mut);
            if (maxElement != blocks.end() && maxElement->timestamp > maxTimestamp) {
                bestBlocks = blocks;
                maxTimestamp = maxElement->timestamp;
            }
        } catch (const exception &e) {
            std::lock_guard<std::mutex> lock(mut);
            error = e;
        } catch (const UserException &e) {
            std::lock_guard<std::mutex> lock(mut);
            error = e.what();
        }
    };

    p2p.broadcast("", makeGetLastRejectedBlocksMessage(countLast), "", function);

    if (bestBlocks.empty() && !error.empty()) {
        throwErr(error);
    } else {
        return bestBlocks;
    }
}

ResponseParse GetNewRejectedBlocksFromServer::parseDumpBlockResponse(bool isCompress, const std::string& result, size_t fromByte, size_t toByte) {
    ResponseParse parsed;
    if (result.empty()) {
        return parsed;
    }

    const auto checked = checkErrorGetRejectedBlockDumpResponse(result);

    if (checked.has_value()) {
        parsed.error = checked.value();
    } else {
        parsed.response = result;
    }
    return parsed;
}

std::vector<std::string> GetNewRejectedBlocksFromServer::getRejectedBlocksDumps(const std::vector<std::vector<unsigned char>> &hashes) {
    if (hashes.empty()) {
        return {};
    }

    const size_t countParts = (hashes.size() + COUNT_BLOCKS_IN_BATCH - 1) / COUNT_BLOCKS_IN_BATCH;

    const auto makeQsAndPost = [&hashes, isCompress=this->isCompress](size_t number) {
        CHECK(hashes.size() > number * COUNT_BLOCKS_IN_BATCH, "Incorrect number");
        const size_t beginBlock = number * COUNT_BLOCKS_IN_BATCH;
        const size_t countBlocks = std::min(COUNT_BLOCKS_IN_BATCH, hashes.size() - beginBlock);
        return std::make_pair("", makeGetRejectedBlocksDumpsMessage(hashes.begin() + beginBlock, hashes.begin() + beginBlock + countBlocks, isCompress));
    };

    const std::vector<std::string> responses = p2p.requests(countParts, makeQsAndPost, "", std::bind(parseDumpBlockResponse, isCompress, _1, _2, _3), {});
    CHECK(responses.size() == countParts, "Incorrect responses");

    std::vector<std::string> result;
    result.reserve(hashes.size());

    for (size_t i = 0; i < responses.size(); i++) {
        const size_t beginBlock = i * COUNT_BLOCKS_IN_BATCH;
        const size_t blocksInPart = std::min(COUNT_BLOCKS_IN_BATCH, hashes.size() - beginBlock);

        const std::vector<std::string> blocks = parseRejectedDumpBlocksBinary(responses[i], isCompress);
        CHECK(blocks.size() == blocksInPart, "Incorrect answer");
        CHECK(beginBlock + blocks.size() <= hashes.size(), "Incorrect answer");
        result.insert(result.end(), blocks.begin(), blocks.end());
    }

    return result;
}

} // namespace torrent_node_lib
