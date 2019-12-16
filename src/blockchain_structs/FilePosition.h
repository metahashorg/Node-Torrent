#ifndef TORRENT_NODE_FILEPOSITION_H
#define TORRENT_NODE_FILEPOSITION_H

#include <string>
#include <vector>

namespace torrent_node_lib {

struct FilePosition {
    size_t pos;
    std::string fileNameRelative;

    FilePosition() = default;

    FilePosition(const std::string &fileNameRelative, size_t pos)
            : pos(pos), fileNameRelative(fileNameRelative) {
    }

    std::string serialize() const;

    void serialize(std::vector<char> &buffer) const;

    static FilePosition deserialize(const std::string &raw, size_t from, size_t &nextFrom);

    static FilePosition deserialize(const std::string &raw, size_t &from);

    static FilePosition deserialize(const std::string &raw);

};

} // namespace torrent_node_lib

#endif //TORRENT_NODE_FILEPOSITION_H
