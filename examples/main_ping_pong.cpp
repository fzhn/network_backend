#include "ping_pong.h"
#include <thread>



int main(){
    network_config config{false, 64 * 1024, 1, TCP, NULL, NULL, NULL, NULL, NULL};
    std::thread server([=](){PingPong(POSIX_SOCKETS, config, true);});
    auto client = PingPong(POSIX_SOCKETS, config, false);
    server.join();
}