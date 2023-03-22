#pragma once
#include <memory>

#include "eventloop.h"
#include "network.h"

void on_data(connection_info hdl, char* data, int size, Network* backend, void * ctx){
    std::cout << data << std::endl;
    std::string s;
    strcmp(data, "Ping!") ? s = "Ping!" : s = "Pong!";
    network_handle handle = hdl;
    std::cout << "Message received on socket: " << hdl.socket << ", sending back: " << s << std::endl;
    try{
      backend->send_data(s.c_str(), s.length(), &handle);
    } catch (const std::exception& e){
      e.what();
    }

}

class PingPong{
  private:
    std::unique_ptr<Network> m_net_backend;
    network_config m_config;
    bool m_server;

  public:   
    PingPong(network_type networkType, network_config config, bool is_server) : m_config(config),
                                                                                m_server(is_server){
                                                                
      log_set_level(4);
      m_config.on_data_cb = on_data;
      m_net_backend = Network::create(networkType, m_config);
      if(m_server){
        network_handle handle = connection_info{{"127.0.0.1", 1337},{"127.0.0.1", 54321}, 0, 0};
        m_net_backend->register_handle(&handle);
        m_net_backend->run("Server");
      } else {
        network_handle handle = connection_info{{"127.0.0.1", 54321},{"127.0.0.1", 1337}, 0, 0};
        std::string s = "Ping!";
        m_net_backend->send_data(s.c_str(), s.length(), &handle);
        m_net_backend->run("client");
      }

    }
};