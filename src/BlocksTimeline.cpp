#include "BlocksTimeline.h"

#include "check.h"
#include "utils/serialize.h"
#include "convertStrings.h"

#include <algorithm>

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
    
    std::lock_guard<std::mutex> lock(mut);
    initialized = true;
}

size_t BlocksTimeline::size() const {
    std::lock_guard<std::mutex> lock(mut);
    return hashes.size();
}

std::pair<size_t, std::vector<char>> BlocksTimeline::addSimpleBlock(const BlockHeader &bh) {
    SimpleBlockElement element;
    element.hash = bh.hash;
    
    std::lock_guard<std::mutex> lock(mut);
    
    CHECK(hashes.find(bh.hash) == hashes.end(), "Element " + toHex(bh.hash) + " already exist");
    
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
    
    CHECK(hashes.find(bh.hash) == hashes.end(), "Element " + toHex(bh.hash) + " already exist");
    
    auto iter = timeline.insert(timeline.end(), element);
    hashes.emplace(element.hash, iter);
    
    signsParent.emplace(element.prevHash, iter);
    
    std::vector<char> serializedData = serializeElement(*iter);
    
    return std::make_pair(hashes.size() - 1, serializedData);
}

std::optional<MinimumSignBlockHeader> BlocksTimeline::findSignForBlock(const Hash &hash) const {
    std::lock_guard<std::mutex> lock(mut);
    
    CHECK(initialized, "Not initialized");
    
    const auto found = signsParent.find(hash);
    if (found == signsParent.end()) {
        return std::nullopt;
    } else {
        const Element &element = *found->second;
        CHECK(std::holds_alternative<SignBlockElement>(element), "Incorrect block element");
        return std::get<SignBlockElement>(element);
    }
}

std::vector<MinimumSignBlockHeader> BlocksTimeline::getSignaturesBetween(const std::optional<Hash> &firstBlock, const std::optional<Hash> &secondBlock) const {
    std::lock_guard<std::mutex> lock(mut);
    
    CHECK(initialized, "Not initialized");
    
    Iterator iterFirst = timeline.cbegin();
    if (firstBlock.has_value()) {
        const auto found = hashes.find(firstBlock.value());
        if (found != hashes.end()) {
            iterFirst = found->second;
        }
    }
    
    Iterator iterSecond = timeline.cend();
    if (secondBlock.has_value()) {
        const auto found = hashes.find(secondBlock.value());
        if (found != hashes.end()) {
            iterSecond = found->second;
        }
    }
    
    std::vector<MinimumSignBlockHeader> result;
    std::for_each(iterFirst, iterSecond, [&result](const Element &element) {
        if (std::holds_alternative<MinimumSignBlockHeader>(element)) {
            result.emplace_back(std::get<MinimumSignBlockHeader>(element));
        }
    });
    
    return result;
}

std::optional<MinimumSignBlockHeader> BlocksTimeline::findSignature(const Hash &hash) const {   
    std::lock_guard<std::mutex> lock(mut);
    
    CHECK(initialized, "Not initialized");
    
    const auto found = hashes.find(hash);
    if (found == hashes.end()) {
        return std::nullopt;
    }
    
    const auto &variant = *found->second;
    if (!std::holds_alternative<MinimumSignBlockHeader>(variant)) {
        return std::nullopt;
    }
    
    return std::get<MinimumSignBlockHeader>(variant);
}

} // namespace torrent_node_lib {
