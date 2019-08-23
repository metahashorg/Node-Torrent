#ifndef SERVER_H_
#define SERVER_H_

#include <mh/mhd/MHD.h>

#include <string>
#include <atomic>

#include "utils/SmallStatistic.h"

namespace torrent_node_lib {
class Sync;
}

class Server: public mh::mhd::MHD {
public:
    
    Server(const torrent_node_lib::Sync &sync, int port, std::atomic<int> &countRunningThreads, const std::string &serverPrivKey) 
        : sync(sync)
        , port(port)
        , serverPrivKey(serverPrivKey)
        , countRunningThreads(countRunningThreads)
        , isStoped(false)
    {}
    
    ~Server() override {}
    
    bool run(int thread_number, Request& mhd_req, Response& mhd_resp) override;
    
    bool init() override;
    
private:
    
    const torrent_node_lib::Sync &sync;
    
    const int port;

    const std::string serverPrivKey;
    
    std::atomic<int> &countRunningThreads;
    
    std::atomic<bool> isStoped;
        
    SmallStatistic smallRequestStatistics;
};

#endif // SERVER_H_
