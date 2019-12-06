#ifndef BLOCKS_TIMELINE_H_
#define BLOCKS_TIMELINE_H_

#include "BlockInfo.h"

#include <map>
#include <list>
#include <variant>
#include <mutex>

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
    
public:
    
    void deserialize(const std::vector<std::pair<size_t, std::string>> &elements);
    
    size_t size() const;
    
public:
    
    std::pair<size_t, std::vector<char>> addSimpleBlock(const BlockHeader &bh);
    
    std::pair<size_t, std::vector<char>> addSignBlock(const SignBlockHeader &bh);
    
    std::optional<MinimumSignBlockHeader> findSignForBlock(const std::vector<unsigned char> &hash) const;
    
private:
    
    mutable std::mutex mut;
    
    List timeline;
    
    std::map<std::vector<unsigned char>, List::iterator> hashes; // TODO переписать на unordered_map
    
    std::map<std::vector<unsigned char>, List::iterator> signsParent;
};
    
} // namespace torrent_node_lib {

#endif // BLOCKS_TIMELINE_H_
