#include "tcmalloc_management.h"

#include <gperftools/malloc_extension.h>

#include "duration.h"

#include <thread>
#include <iostream>

namespace common {

void tcmallocMaintenance() {
    std::thread th([]{
        while (true) {
            Timer tt;
            MallocExtension::instance()->ReleaseFreeMemory();
            tt.stop();
            
            if (tt.count() >= 5s) {
                std::cout << "Release tcmalloc memory milliseconds: " << tt.countMs() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });
    th.detach();
}

} // namespace common {
