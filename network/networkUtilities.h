#pragma once
#include <sys/eventfd.h>
#include <unistd.h>
#include <rdma/fi_domain.h>
extern "C" {
  #include "log.h"
}

#include <cstring>

class Network;

enum network_mode {TCP, UDP, RDMA, SHMEM};

struct socket{
    int ID;
};


// Posix

struct socket_addr{
    std::string ip;
    unsigned short port;
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
    network_mode mode;
};

// Libfabric

struct listen_socket;
struct send_socket;
struct recv_socket;
struct endpoint;
struct domain_ctx{
  struct fid_fabric fabric;
  struct fid_domain domain;

  uint64_t req_key;
  uint16_t reg_mr;
  std::vector<fid> mr;

  int nb_sockets;
};

struct connection_info_libfarbic{
    const socket_addr local_addr;
    const socket_addr remote_addr;
    listen_socket* lsocket;
    send_socket* ssocket;
    recv_socket* rsocket;
    network_mode mode;
    size_t buf_size;
    size_t num_buf;
};

// General data structures

enum network_type {NONE, POSIX_SOCKETS, LIBFABRIC, UCX};

struct buf_struct {
    char* buf;
    size_t size;
    size_t pos;
    size_t left_to_read;
    std::vector<char> buf_array;
};

struct network_config {
    bool zero_copy;
    size_t num_buffers;
    size_t buffersize;
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



