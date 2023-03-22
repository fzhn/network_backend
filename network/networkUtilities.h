#pragma once
#include <sys/eventfd.h>
#include <unistd.h>
#include <rdma/fi_domain.h>
extern "C" {
  #include "log.h"
}

#include <cstring>

class Network;

struct socket{
    int ID;
};

struct socket_addr{
    std::string ip;
    unsigned short port;
    bool operator=(const socket_addr& rhs) const
    {
        if (ip == rhs.ip && port == rhs.port)
        {
           return true;
        }
        return false;
    }
    bool operator<(const socket_addr& rhs) const
    {
        if (port < rhs.port)
        {
           return true;
        }
        return false;
    }
};

struct connection_info{
    const socket_addr local_addr;
    const socket_addr remote_addr;
    int lsocket;
    int socket;
};

struct listen_socket;
struct send_socket;
struct domain_ctx{
  struct fid_fabric fabric;
  struct fid_domain domain;

  uint64_t req_key;
  uint16_t reg_mr;
  std::vector<fid> mr;

  int nb_sockets;
};

enum network_type {NONE, POSIX_SOCKETS, LIBFABRIC, UCX};

enum network_mode {TCP, UDP, RDMA, SHMEM};

struct network_config {
    bool zero_copy;
    uint64_t num_buffers;
    uint64_t buffersize;
    network_mode mode;
    void* usr_ctx;

    //Callbacks
    std::function<void(connection_info info, char* data, int size, Network* backend, void* usr_ctx)> on_data_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> on_connnection_established_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> on_connection_closed_cb;
    std::function<void(connection_info info,Network* backend, void* usr_ctx)> on_connection_refused_cb;
};


//enum resource_type {TIMER, SIGNAL, LIBFABRIC_FD};
//enum socket_type {BSEND, USEND, BRECV, URECV, BSUB, USUB, BPUB, UPUB, BLISTEN, ULISTEN, NOSOCKET, TESTSOCKET};



