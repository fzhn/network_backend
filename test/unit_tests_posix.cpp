#define CATCH_CONFIG_MAIN
#define UNIT_TESTING

#include "catch.hpp"
#include "network.h"
#include <string>
#include <thread>


TEST_CASE( "Test init function", "[init_test]" ) {
    network_config config{false, 64, 1, TCP, NULL, NULL, NULL, NULL};
    std::unique_ptr<Network> net_backend = Network::create(POSIX_SOCKETS, config);

    SECTION("Uninitialized handle") {
        network_handle non_init_handle;
        REQUIRE_THROWS_AS(net_backend->register_handle(&non_init_handle), std::invalid_argument);
    }
    SECTION("Empty handle") {
        network_handle empty_handle = connection_info{{"", 0},{"", 0}, 0, 0};
        REQUIRE_THROWS_AS(net_backend->register_handle(&empty_handle), std::runtime_error);
    }
    SECTION("Working example") {
        network_handle working_handle = connection_info{{"127.0.0.1", 1337},{"127.0.0.1", 54321}, -1, -1};
        net_backend->register_handle(&working_handle);
        REQUIRE(std::get<connection_info>(working_handle).lsocket >= 0);
    }
}

TEST_CASE( "Test closing handle", "[close_handle_test]" ) {
    network_config config{false, 64, 1, TCP, NULL, NULL, NULL, NULL};
    std::unique_ptr<Network> net_backend = Network::create(POSIX_SOCKETS, config);
    network_handle handle = connection_info{{"127.0.0.1", 7331},{"127.0.0.1", 54321}, -1, 1};

    SECTION("Uninitialized handle") {
        network_handle non_init_handle;
        REQUIRE_THROWS_AS(net_backend->close_handle(&non_init_handle), std::invalid_argument);
    }
    SECTION("Empty handle") {
        network_handle empty_handle = connection_info{{"", 0},{"", 0}, -1, -1};
        REQUIRE_THROWS_AS(net_backend->close_handle(&empty_handle), std::invalid_argument);
    }
    SECTION("Working example") {
        net_backend->register_handle(&handle);
        REQUIRE(std::get<connection_info>(handle).lsocket >= 0);
        net_backend->close_handle(&handle);
        REQUIRE(std::get<connection_info>(handle).lsocket < 0);
        REQUIRE(std::get<connection_info>(handle).socket < 0);
    }
}


TEST_CASE( "Test send", "[send_test]" ) {
    network_config config{false, 64, 1, TCP, NULL, NULL, NULL, NULL};
    std::unique_ptr<Network> net_backend = Network::create(POSIX_SOCKETS, config);
    std::string s = "Hello";
    
    SECTION("Uninitialized handle:") {
        network_handle non_init_handle;
        REQUIRE_THROWS_AS(net_backend->send_data(s.c_str(), s.length(), &non_init_handle), std::invalid_argument);
    }
    SECTION("Empty handle") {
        network_handle empty_handle = connection_info{{"", 0},{"", 0}, -1, -1};
        REQUIRE_THROWS_AS(net_backend->send_data(s.c_str(), s.length(), &empty_handle), std::runtime_error);
    }
    SECTION("Send to non existing remote:") {
        network_handle handle = connection_info{{"127.0.0.1", 4444},{"127.0.0.1", 1907}, -1, -1};
        REQUIRE_THROWS_AS(net_backend->send_data(s.c_str(), s.length(), &handle), std::runtime_error);
    }
    SECTION("Send to non existing remote:") {   
        std::thread server([](){
            network_config config{false, 64, 1, TCP, NULL, NULL, NULL, NULL};
            std::unique_ptr<Network> server_net_backend = Network::create(POSIX_SOCKETS, config);
            network_handle recv_handle = connection_info{{"127.0.0.1", 1907},{"", 0}, -1, -1};
            server_net_backend->register_handle(&recv_handle);
            REQUIRE(std::get<connection_info>(recv_handle).lsocket >= 0);
            sleep(2);
        });
        sleep(1);
        network_handle handle = connection_info{{"127.0.0.1", 4444},{"127.0.0.1", 1907}, -1, -1};
        REQUIRE(net_backend->send_data(s.c_str(), s.length(), &handle) == s.length());
        server.join();
    }
    SECTION("Exceed receiving buffer") { 
        std::thread server([](){
            network_config config{false, 64, 1, TCP, NULL, NULL, NULL, NULL};
            std::unique_ptr<Network> server_net_backend = Network::create(POSIX_SOCKETS, config);
            network_handle recv_handle = connection_info{{"127.0.0.1", 1607},{"", 0}, -1, -1};
            server_net_backend->register_handle(&recv_handle);
            REQUIRE(std::get<connection_info>(recv_handle).lsocket >= 0);
            sleep(2);
        });
        sleep(1);
        char * test= (char*)calloc(1, 1024*1024*10);
        network_handle handle = connection_info{{"127.0.0.1", 4444},{"127.0.0.1", 1607}, -1, -1};
        REQUIRE(net_backend->send_data(test, 10 * 1024 * 1024, &handle) == 2539008); //2539008 seems to be the maximum a socket can send in one go if the recv buffer is not empty.
        REQUIRE(std::get<connection_info>(handle).socket >= 0);
        network_handle sock_handle = connection_info{{"", 0},{"", 0}, -1, std::get<connection_info>(handle).socket};
        REQUIRE(net_backend->send_data(test, 10 * 1024 * 1024, &handle) == 1942594); //This amount of data is send to the receiver in the meanwhile. Since it is not processed all buffers are full now. This test will use the already existing socket by addr
        REQUIRE(net_backend->send_data(test, 10 * 1024 * 1024, &sock_handle) == -1); //Send returns an error. This test will use the already existing socket by FD
        REQUIRE(errno == 11); //Which should be EAGAIN
        server.join();
    }
}
