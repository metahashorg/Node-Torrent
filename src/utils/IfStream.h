#ifndef IF_STREAM_H_
#define IF_STREAM_H_

#include <memory>
#include <string>
#include <functional>

namespace torrent_node_lib {

class IfStream {
public:
    
    void open(const std::string &file);
    
    void reopen(const std::string &file);
    
    void close();
    
    void seek(size_t pos);
    
    size_t fileSize() const;
    
    void read(char *data, size_t size);
    
private:
    
    std::unique_ptr<FILE, std::function<void(FILE*)>> fd;
};

} // namespace torrent_node_lib {

#endif // IF_STREAM_H_
