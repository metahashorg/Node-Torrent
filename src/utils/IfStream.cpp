#include "IfStream.h"

#include "check.h"

#include <string.h>

using namespace common;

namespace torrent_node_lib {

static void closeFd(FILE* fd) {
    fclose(fd);
}

void IfStream::open(const std::string &file) {
    fd = std::unique_ptr<FILE, std::function<void(FILE*)>>(fopen(file.c_str(), "rb"), &closeFd);
    CHECK(fd != nullptr, "Not opened file " + file + ": " + strerror(errno));
    fileName = file;
}

void IfStream::reopen(const std::string &file) {
    open(file);
}

void IfStream::close() {
    fd.reset();
}

void IfStream::seek(size_t pos) {
    CHECK(fd != nullptr, "File not opened");
    clearerr(fd.get());
    const int res = fseek(fd.get(), pos, SEEK_SET);
    CHECK(res == 0, "Not seekeng " + fileName + " " + std::to_string(pos));
    const size_t currPos = ftell(fd.get());
    CHECK(currPos == pos, "Not seeking " + fileName + " " + std::to_string(pos));
}

size_t IfStream::fileSize() const {
    CHECK(fd != nullptr, "File not opened");
    clearerr(fd.get());
    const int res = fseek(fd.get(), 0, SEEK_END);
    CHECK(res == 0, "Not seekeng " + fileName + " " + std::to_string(res));
    return ftell(fd.get());
}

void IfStream::read(char *data, size_t size) {
    CHECK(fd != nullptr, "File not opened");
    const size_t readed = fread(data, 1, size, fd.get());
    (void)readed;
}

} // namespace torrent_node_lib {
