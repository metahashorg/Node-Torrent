#ifndef HASHED_STRING_H_
#define HASHED_STRING_H_

#include <string>

namespace common {

struct HashedString {
    std::string s;
    std::size_t hashString;
    
    HashedString() = default;
    
    HashedString(const std::string &s)
        : s(s)
        , hashString(std::hash<std::string_view>()(s))
    {}
    
    HashedString(const char *data, size_t size)
        : s(data, size)
        , hashString(std::hash<std::string_view>()(std::string_view(data, size)))
    {}
    
    HashedString(const unsigned char *data, size_t size)
        : s((const char*)(data), size)
        , hashString(std::hash<std::string_view>()(std::string_view((const char*)(data), size)))
    {}
    
    HashedString(const HashedString &) = default;
    HashedString(HashedString&&) = default;
    HashedString& operator=(const HashedString&) = default;
    HashedString& operator=(HashedString&&) = default;
    
    bool operator==(const HashedString &other) const {
        return this->s == other.s;
    }
    
    operator std::string() const {
        return s;
    }
};

}

namespace std {
    
template <>
struct hash<common::HashedString> {
    std::size_t operator()(const common::HashedString& k) const noexcept {
        return k.hashString;
    }
};
    
}

#endif // HASHED_STRING_H_
