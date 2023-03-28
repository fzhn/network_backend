#pragma once
#include <memory>

#include "eventloop.h"
#include "network.h"

void on_data(connection_info hdl, char* data, int size, Network* backend, void * ctx){
    std::string s;
    std::string data_string(data, size);
    strcmp(data_string.c_str(), "Ping!") ? s = "Ping!" : s = "Pong!";
    network_handle handle = hdl;
    std::cout << "Message \"" << data_string << "\" received on socket: " << hdl.socket << ", sending back: " << s << std::endl;
    usleep(100000);
    try{
      backend->send_data(s.c_str(), s.length()+1, &handle);
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
        network_handle handle = connection_info{{"127.0.0.1", 1337},{"127.0.0.1", 54321}, 0, 0, TCP};
        m_net_backend->register_handle(&handle);
        m_net_backend->run("Server");
      } else {
        network_handle handle = connection_info{{"127.0.0.1", 54321},{"127.0.0.1", 1337}, 0, 0, TCP};
        size_t test = 0004;
        char header[sizeof(size_t)];
        snprintf(header, sizeof(header), "%zu", test);
        char teststring[128];
        strcpy(teststring, "PingPong!");
        strcat(teststring, "00000004");
        strcat(teststring, "Bam!");
        //std::string s = "PingPong!" + header + "Bam!";
        int ret = m_net_backend->send_data(teststring, 21, &handle);
        std::cout << "Initial send returned: " << ret << " last char " << std::endl;
        for(int i = 0; i <21; i++){
          std::cout << teststring[i];
        }
        std::cout << std::endl;

        m_net_backend->run("client");
      }

    }
};