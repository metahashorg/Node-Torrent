#include "get_rejected_blocks_messages.h"

#include "utils/serialize.h"
#include "jsonUtils.h"
#include "check.h"
#include "convertStrings.h"
#include "utils/compress.h"

using namespace common;

namespace torrent_node_lib {

std::string makeGetLastRejectedBlocksMessage(size_t count) {
    return "\"method\": \"get-rejected-blocks\", \"params\": {\"count\": " + std::to_string(count) + "}";
}

std::string makeGetRejectedBlocksDumpsMessage(std::vector<std::vector<unsigned char>>::const_iterator hashesBegin, std::vector<std::vector<unsigned char>>::const_iterator hashesEnd, bool isCompress) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();

    rapidjson::Value nodesJson(rapidjson::kArrayType);
    for (auto iter = hashesBegin; iter != hashesEnd; iter++) {
        nodesJson.PushBack(strToJson(toHex(*iter), allocator), allocator);
    }
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("hashes", nodesJson, allocator);
    resultJson.AddMember("isCompress", isCompress, allocator);

    doc.AddMember("params", resultJson, allocator);
    doc.AddMember("method", "get-rejected-dumps", allocator);
    return jsonToString(doc, false);
}

std::vector<RejectedBlockMessage> parseGetLastRejectedBlocksMessage(const std::string &message) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(message.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + message);

    const auto &jsonParams = get<JsonObject>(doc, "result");

    const auto &elementsJson = get<JsonArray>(jsonParams, "elements");
    std::vector<RejectedBlockMessage> result;
    std::transform(elementsJson.Begin(), elementsJson.End(), std::back_inserter(result), [](const auto &elementJson) {
        const auto &eJson = get<JsonObject>(elementJson);
        RejectedBlockMessage r;
        r.blockNumber = get<size_t>(eJson, "blockNumber");
        r.timestamp = get<size_t>(eJson, "timestamp");
        const std::string hash = get<std::string>(eJson, "hash");
        r.hash = fromHex(hash);

        return r;
    });

    return result;
}

std::vector<std::string> parseRejectedDumpBlocksBinary(const std::string &response, bool isCompress) {
    std::vector<std::string> res;
    const std::string r = isCompress ? decompress(response) : response;
    size_t from = 0;
    while (from < r.size()) {
        res.emplace_back(deserializeStringBigEndian(r, from));
    }
    return res;
}

std::optional<std::string> checkErrorGetRejectedBlockDumpResponse(const std::string &response) {
    if (response.size() <= 512 && response[0] == '{' && response[response.size() - 1] == '}') {
        try {
            rapidjson::Document doc;
            const rapidjson::ParseResult pr = doc.Parse(response.c_str());
            CHECK(pr, "rapidjson parse error. Data: " + response);

            CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
        } catch (const exception &e) {
            return e;
        }
    }

    return std::nullopt;
}

} // namespace torrent_node_lib {
