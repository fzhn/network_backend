#include "ping_pong.h"
#include <thread>



int main(){
    network_config config{false, 64, 64 *1024, TCP, NULL, NULL, NULL, NULL, NULL};
    std::thread server([=](){PingPong(POSIX_SOCKETS, config, true);});
    sleep(1);
    auto client = PingPong(POSIX_SOCKETS, config, false);
    server.join();
}