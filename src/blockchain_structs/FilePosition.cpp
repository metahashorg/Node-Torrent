#include "FilePosition.h"

#include <string>
#include <vector>
#include "duration.h"
#include "check.h"
#include "utils/serialize.h"

namespace torrent_node_lib {

std::string FilePosition::serialize() const {
    CHECK(!fileNameRelative.empty(), "FilePosition not initialized");

    std::string res;
    res.reserve(50);
    res += torrent_node_lib::serializeString(fileNameRelative);
    res += torrent_node_lib::serializeInt<size_t>(pos);
    return res;
}

void FilePosition::serialize(std::vector<char> &buffer) const {
    CHECK(!fileNameRelative.empty(), "FilePosition not initialized");
    torrent_node_lib::serializeString(fileNameRelative, buffer);
    torrent_node_lib::serializeInt<size_t>(pos, buffer);
}

FilePosition FilePosition::deserialize(const std::string &raw, size_t from, size_t &nextFrom) {
    FilePosition result;

    result.fileNameRelative = torrent_node_lib::deserializeString(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;

    result.pos = torrent_node_lib::deserializeInt<size_t>(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;

    return result;
}

FilePosition FilePosition::deserialize(const std::string &raw, size_t &from) {
    size_t endPos = from;
    const FilePosition res = deserialize(raw, from, endPos);
    CHECK(endPos != from, "Incorrect raw");
    from = endPos;
    return res;
}

FilePosition FilePosition::deserialize(const std::string &raw) {
    size_t tmp;
    return deserialize(raw, 0, tmp);
}

} // namespace torrent_node_lib
