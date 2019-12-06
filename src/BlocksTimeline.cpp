#include "BlocksTimeline.h"

#include "check.h"
#include "utils/serialize.h"

using namespace common;

namespace torrent_node_lib {
    
void BlocksTimeline::SimpleBlockElement::serialize(std::vector<char> &buffer) const {
    CHECK(!hash.empty(), "empty hash");
    
    serializeVector(hash, buffer);
}
    
BlocksTimeline::SimpleBlockElement BlocksTimeline::SimpleBlockElement::deserialize(const std::string &raw, size_t &fromPos) {
    SimpleBlockElement result;
    result.hash = deserializeVector(raw, fromPos);
    
    return result;
}

std::vector<char> serializeElement(const BlocksTimeline::Element &element) {
    std::vector<char> result;
    serializeVariant(element, result);
    return result;
}

void BlocksTimeline::deserialize(const std::vector<std::pair<size_t, std::string>> &elements) {
    for (const auto &[number, element]: elements) {
        CHECK(number == hashes.size(), "Incorrect sequence");
        
        Element el;
        size_t from = 0;
        deserializeVarint(element, from, el);
        
        auto iter = timeline.insert(timeline.end(), el);
        std::visit([this, iter](const auto &e) {
            hashes.emplace(e.hash, iter);
        }, el);
        
        if (std::holds_alternative<SignBlockElement>(el)) {
            const SignBlockElement &block = std::get<SignBlockElement>(el);
            signsParent.emplace(block.prevHash, iter);
        }
    }
}

size_t BlocksTimeline::size() const {
    std::lock_guard<std::mutex> lock(mut);
    return hashes.size();
}

std::pair<size_t, std::vector<char>> BlocksTimeline::addSimpleBlock(const BlockHeader &bh) {
    SimpleBlockElement element;
    element.hash = bh.hash;
    
    std::lock_guard<std::mutex> lock(mut);
    
    auto iter = timeline.insert(timeline.end(), element);
    hashes.emplace(element.hash, iter);
    
    std::vector<char> serializedData = serializeElement(*iter);
    
    return std::make_pair(hashes.size() - 1, serializedData);
}

std::pair<size_t, std::vector<char>> BlocksTimeline::addSignBlock(const SignBlockHeader &bh) {
    SignBlockElement element;
    element.hash = bh.hash;
    element.prevHash = bh.prevHash;
    element.filePos = bh.filePos;
    
    std::lock_guard<std::mutex> lock(mut);
    
    auto iter = timeline.insert(timeline.end(), element);
    hashes.emplace(element.hash, iter);
    signsParent.emplace(element.prevHash, iter);
    
    std::vector<char> serializedData = serializeElement(*iter);
    
    return std::make_pair(hashes.size() - 1, serializedData);
}

std::optional<MinimumSignBlockHeader> BlocksTimeline::findSignForBlock(const std::vector<unsigned char> &hash) const {
    std::lock_guard<std::mutex> lock(mut);
    
    const auto found = signsParent.find(hash);
    if (found == signsParent.end()) {
        return std::nullopt;
    } else {
        const Element &element = *found->second;
        CHECK(std::holds_alternative<SignBlockElement>(element), "Incorrect block element");
        return std::get<SignBlockElement>(element);
    }
}

} // namespace torrent_node_lib {
