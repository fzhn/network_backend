#include "network.h"


std::unique_ptr<Network> Network::create(network_type type, network_config& config){
    if (type == LIBFABRIC){
        return std::make_unique<NetworkLibfabric>(config);
    } else if (type == UCX) {
        return std::make_unique<NetworkUCX>(config);
    } else if (type == POSIX_SOCKETS){
        return std::make_unique<NetworkPosix>(config);
    } else {
        log_error("Error: unknown network type. Using unix sockets as fallback.");
        return std::make_unique<NetworkPosix>(config);
    }
}