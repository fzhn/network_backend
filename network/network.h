#pragma once
#include <iostream>
#include <variant>
#include <memory>
#include <vector>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

#include "networkUtilities.h"
#include "eventloop.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

using network_handle = std::variant<std::monostate, connection_info>;

class Network{
public:
    network_config m_config;
    Network(network_config config) : m_config(config){}
    static std::unique_ptr<Network> create(network_type type, network_config& config);
    virtual void run() = 0;
    virtual void register_handle(network_handle* handle) = 0;
    virtual void close_handle(network_handle* handle) = 0;
    virtual ssize_t send_data(const char* data, size_t size, network_handle* handle) = 0;
};

class NetworkLibfabric: public Network{
public:
    NetworkLibfabric(network_config& config);
    ~NetworkLibfabric();
    void run() override; 
    void register_handle(network_handle* handle) override;
    void close_handle(network_handle* handle) override;
    ssize_t send_data(const char* data, size_t size, network_handle* handle) override;
private:
    Eventloop m_evloop;
    domain_ctx m_send_domain;
    fid_fabric m_fabric;
    fi_info* m_info;
    void init_listen(listen_socket* socket, connection_info* connection);
    void init_send(send_socket* socket, connection_info* connection);
    void register_buffers(recv_socket* rsocket);
    void on_listen_socket_cm_event(int,void*);
    void on_send_socket_cm_event(int,void*);
    int read_cm_event(struct fid_eq* eq, struct fi_info** info, struct fi_eq_err_entry* err_entry);
    void handle_connreq();
    int connect_endpoint(endpoint* ep, fid_domain* domain);
    std::map<socket_addr, listen_socket> m_lsockets;
    std::map<socket_addr, send_socket> m_ssockets;  
};


class NetworkUCX: public Network {
public:
    NetworkUCX(network_config& config);
    ~NetworkUCX();
    void run() override;
    void register_handle(network_handle* handle) override;
    void close_handle(network_handle* handle) override;
    ssize_t send_data(const char* data, size_t size, network_handle* handle) override;
private:
    void establish_oob_connection(network_handle* handle);
};

class NetworkPosix: public Network {
public:
    NetworkPosix(network_config& config);
    ~NetworkPosix();
    void run() override;
    void register_handle(network_handle* handle) override;
    void close_handle(network_handle* handle) override;
    ssize_t send_data(const char* data, size_t size, network_handle* handle) override;
private:
    Eventloop m_evloop;
    std::mutex m_mux;
    size_t m_buffersize;
    std::vector<int> m_lsockets;
    std::map<socket_addr, int> m_connection_map;
    std::map<int, std::mutex> m_send_mutex_map;
    std::map<int, buf_struct> m_buf_map;
    //Callbacks
    std::function<void(connection_info info, char* data, int size, Network* backend, void* usr_ctx)> m_on_data_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> m_on_connnection_established_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> m_on_connection_closed_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> m_on_connection_refused_cb;

    int createListenSocket(connection_info* connection);
    void check_recv_return(int ret);
    void send_connect(connection_info& handle);
    void close_handle_ev(int fd, void* data);
    void on_connection_request(int lsocket, void* data);
    void on_connection_closed(int socket, void* data);
    void on_data(int socket, void* data);
};
