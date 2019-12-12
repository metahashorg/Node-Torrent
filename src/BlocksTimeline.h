#ifndef BLOCKS_TIMELINE_H_
#define BLOCKS_TIMELINE_H_

#include "BlockInfo.h"

#include <map>
#include <list>
#include <variant>
#include <mutex>
#include <functional>

namespace torrent_node_lib {
       
class BlocksTimeline {
public:
    
    struct SimpleBlockElement {
        std::vector<unsigned char> hash;
        
        void serialize(std::vector<char> &buffer) const;
        
        static SimpleBlockElement deserialize(const std::string &raw, size_t &fromPos);
        
    };
    
    using SignBlockElement = MinimumSignBlockHeader;
    
public:
    
    using Element = std::variant<SignBlockElement, SimpleBlockElement>;
    
    using List = std::list<Element>;
    
    using Iterator = List::const_iterator;
    
    using Hash = std::vector<unsigned char>;
    
public:
    
    void deserialize(const std::vector<std::pair<size_t, std::string>> &elements);
    
    size_t size() const;
    
public:
    
    std::pair<size_t, std::vector<char>> addSimpleBlock(const BlockHeader &bh);
    
    std::pair<size_t, std::vector<char>> addSignBlock(const SignBlockHeader &bh);
    
    std::optional<MinimumSignBlockHeader> findSignForBlock(const Hash &hash) const;
    
    std::vector<MinimumSignBlockHeader> getSignaturesBetween(const std::optional<Hash> &firstBlock, const std::optional<Hash> &secondBlock) const;
    
    std::optional<MinimumSignBlockHeader> findSignature(const Hash &hash) const;
    
    template<typename T>
    void filter(std::vector<T> &elements, const std::function<Hash(const T &t)> &getter) const {
        std::lock_guard<std::mutex> lock(mut);
        
        elements.erase(std::remove_if(elements.begin(), elements.end(), [this, &getter](const T &element) {
            return hashes.find(getter(element)) != hashes.end();
        }), elements.end());
    }
    
private:
    
    mutable std::mutex mut;
    
    bool initialized = false;
    
    List timeline;
    
    std::map<Hash, Iterator> hashes; // TODO переписать на unordered_map
    
    std::map<Hash, Iterator> signsParent;
};
    
} // namespace torrent_node_lib {

#endif // BLOCKS_TIMELINE_H_
