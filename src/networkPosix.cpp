#include "network.h"
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

NetworkPosix::NetworkPosix(network_config& config) :    Network(config),
                                                    m_evloop(Eventloop()),
                                                    m_buffersize(config.buffersize),
                                                    m_on_data_cb(config.on_data_cb),
                                                    m_on_connnection_established_cb(config.on_connnection_established_cb),
                                                    m_on_connection_closed_cb(config.on_connection_closed_cb),
                                                    m_on_connection_refused_cb(config.on_connection_refused_cb)

{
    log_set_level(LOG_INFO);
    if (config.mode != TCP){
        log_error("UNIX sockets only support TCP. Continue with using TCP.");
    }
}

NetworkPosix::~NetworkPosix(){
    std::for_each(m_lsockets.begin(), m_lsockets.end(), [=](auto& lsocket){m_evloop.remove_fd(lsocket);});
    std::for_each(m_connection_map.begin(), m_connection_map.end(), [=](auto& connection){m_evloop.remove_fd(connection.second);});
}


int NetworkPosix::createListenSocket(connection_info* connection){
    int listensocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listensocket == -1)
    {
        log_error("Cannot create TCP listen socket");
        return -1;
    }
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(connection->local_addr.port);
    inet_pton(AF_INET, connection->local_addr.ip.c_str(), &local_addr.sin_addr);
    if (bind(listensocket, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1) 
    {
        log_error("Can't bind to IP/port");
        return -1;
    }

    if (listen(listensocket, SOMAXCONN) == -1)
    {
        log_error("Can't listen !");
        return -1;
    }
    std::cout << "Listening at address: " << inet_ntoa(local_addr.sin_addr) << " and port: " << ntohs(local_addr.sin_port) << std::endl;
    connection->lsocket = listensocket;
    m_lsockets.push_back(listensocket);
    auto ctx = ev_context{listensocket, NULL, std::bind(&NetworkPosix::on_connection_request, this, std::placeholders::_1, std::placeholders::_2)};
    m_evloop.register_fd(ctx);
    return listensocket;
}

void NetworkPosix::on_connection_request(int lsocket, void* data){
    sockaddr_in remote;
    socklen_t remoteSize = sizeof(remote);
    int socket = accept(lsocket, (struct sockaddr *)&remote, &remoteSize);
    if (socket == -1)
    {
        log_error("Can't connect with remote");
        if(m_on_connection_refused_cb != NULL){
            m_on_connection_refused_cb(connection_info{{"", 0},{inet_ntoa(remote.sin_addr), ntohs(remote.sin_port)}, lsocket, -1}, this, m_config.usr_ctx);
        }
        return;
    }
    std::cout << "Remote address: " << inet_ntoa(remote.sin_addr) << " and port: " << ntohs(remote.sin_port) << std::endl;

    m_buf_map[socket].pos = 0;
    m_buf_map[socket].left_to_read = 0;
    m_buf_map[socket].size = m_buffersize;
    m_buf_map[socket].buf_array.resize(m_buffersize, '\0');
    m_buf_map[socket].buf = m_buf_map[socket].buf_array.data();

    auto ctx = ev_context{socket, NULL, std::bind(&NetworkPosix::on_data, this, std::placeholders::_1, std::placeholders::_2), std::bind(&NetworkPosix::on_connection_closed, this, std::placeholders::_1, std::placeholders::_2)};
    m_evloop.register_fd(ctx);
    m_connection_map[socket_addr{inet_ntoa(remote.sin_addr), ntohs(remote.sin_port)}] = socket;
    if(m_on_connnection_established_cb != NULL){
        m_on_connnection_established_cb(connection_info{{"", 0},{inet_ntoa(remote.sin_addr), ntohs(remote.sin_port)}, lsocket, socket}, this, m_config.usr_ctx);
    }
}

void NetworkPosix::on_connection_closed(int socket, void* data){
    connection_info handle{{"", 0},{"", 0}, -1, socket};
    if(m_on_connection_closed_cb != NULL){
        m_on_connection_closed_cb(handle, this, m_config.usr_ctx);
    }
    log_info("on_connection_closed: %d", socket);
    auto pos = std::find_if(m_connection_map.begin(), m_connection_map.end(), [=](auto& connection){return connection.second == handle.socket;});
    if(pos != m_connection_map.end()){ //need to check manually, since std::map::erase() does not take map.end()
        m_connection_map.erase(pos);
    }
    handle.socket = -1;
}

void NetworkPosix::check_recv_return(int ret){
    if (ret == -1){
        log_error("There was a connection issue for the %s", m_name.c_str());
    } else if (ret < 0){
        log_error("Unexpected problem.");
        char buffer[256];
        std::cout << "Recv of  " << m_name << " returned: " << ret;
        char * errMsg = strerror_r(errno, buffer, 256);
        std::cout << ": " << errMsg << std::endl;
    }
} 


void NetworkPosix::on_data(int socket, void* data){
    if(m_buf_map.count(socket) == 0){
        log_error("No buffer associated to this socket: %d.", socket);
        return;
    } 
    auto buf_data = &m_buf_map.at(socket);
    char* buf = buf_data->buf + buf_data->pos;
    int bytes_recv = recv(socket, buf, buf_data->size - buf_data->pos, MSG_DONTWAIT) + buf_data->pos; 
    log_debug("%d bytes read (including leftover form previous read)(size: %d, position: %d) \n", bytes_recv, buf_data->size, buf_data->pos);
    if (bytes_recv <= 0) {
        check_recv_return(bytes_recv);
        buf_data->pos = 0;
        buf_data->left_to_read = 0;
        return;
    }
    size_t current_pos = 0;
    while (current_pos < bytes_recv){
        log_debug("Entering while loop for new iteration. Starting at: %d, total bytes received: %d", current_pos, bytes_recv);
        if ((bytes_recv - current_pos) < sizeof(size_t)){
            std::copy(buf_data->buf + current_pos, buf_data->buf + bytes_recv, buf_data->buf);
            buf_data->left_to_read = sizeof(size_t) - bytes_recv;
            buf_data->pos = bytes_recv;
            return;
        }
        size_t msg_size = std::strtol(buf_data->buf + current_pos, NULL, 10);
        if ((bytes_recv - current_pos - sizeof(size_t)) < msg_size){
            if(current_pos > 0){
                std::copy(buf_data->buf + current_pos, buf_data->buf + bytes_recv, buf_data->buf);
            }

            buf_data->left_to_read = sizeof(size_t) - bytes_recv;
            buf_data->pos = bytes_recv - current_pos;
            return;
        } else {
            m_config.on_data_cb(connection_info{{"", 0},{"", 0}, -1, socket, TCP}, buf_data->buf + current_pos + sizeof(size_t), msg_size, this, m_config.usr_ctx);
        }
        current_pos += sizeof(size_t) + msg_size;
    }
    buf_data->pos = 0;
    buf_data->left_to_read = 0;
    std::memset(buf_data->buf, '\0', m_buffersize);
    return;
}



void NetworkPosix::run(std::string name){
    m_name = name;
    m_evloop.evloop_run();
}

void NetworkPosix::register_handle(network_handle* handle){
    std::visit(
    overloaded{[&](connection_info& handle) {
           if ( createListenSocket(&handle) < 0){
               throw std::runtime_error("Could not establish listen socket");
           }
        },
        [](std::monostate& handle) {
            throw std::invalid_argument("Network handle is not initialized");
        },
        [](auto& handle) {
            throw std::invalid_argument("Wrong handle for this netwokr provider. Expecting TCP handle.");
        },
    }, *handle);
}


void NetworkPosix::close_handle(network_handle* handle){
    std::visit(
    overloaded{[this](connection_info& handle) {
            if (handle.socket < 0 && handle.lsocket < 0){
                throw std::invalid_argument("Empty network handle.");
            }
            std::unique_lock<std::mutex> lck(m_mux);
            if(handle.lsocket >= 0){
                m_evloop.remove_fd(handle.lsocket);
                m_lsockets.erase(std::remove(m_lsockets.begin(), m_lsockets.end(), handle.lsocket), m_lsockets.end());
                handle.lsocket = -1;
            }
            if(handle.socket >= 0){
                m_evloop.remove_fd(handle.socket);
                //m_sockets.erase(std::remove(m_sockets.begin(), m_sockets.end(), handle.socket), m_sockets.end());
                auto pos = std::find_if(m_connection_map.begin(), m_connection_map.end(), [=](auto& connection){return connection.second == handle.socket;});
                if(pos != m_connection_map.end()){ //need to check manually, since std::map::erase() does not take map.end()
                    m_connection_map.erase(pos);
                }
                handle.socket = -1;
            } 
        },
        [](std::monostate& handle) {
            throw std::invalid_argument("Network handle is not initialized");
        },
        [](auto& handle) {
            throw std::invalid_argument("Wrong handle for this netwokr provider. Expecting TCP handle.");
        },
    }, *handle);
}

void NetworkPosix::send_connect(connection_info& handle){
    auto sock = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
    if (sock < 0){
        throw std::runtime_error("Cannot create socket.");
    }
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(handle.remote_addr.port);
    //std::cout << "Connecting to " << handle.remote_addr.ip << ":" << handle.remote_addr.port << std::endl;
    inet_pton(AF_INET, handle.local_addr.ip.c_str(), &remote_addr.sin_addr);
    bool try_connecting = true;
    while(try_connecting){
        int ret = connect(sock, (struct sockaddr*)&remote_addr, sizeof(remote_addr));
        if (ret < 0) {
            if (errno == EINPROGRESS /*|| errno == ECONNREFUSED*/){continue;}
            std::cout << "Remote address: " << inet_ntoa(remote_addr.sin_addr) << " and port: " << ntohs(remote_addr.sin_port) << " socket: " << sock << std::endl;
            throw std::runtime_error("Could not establish connection to remote, Errno: " + std::to_string(errno));
        }
        try_connecting = false;
    }
    {
        std::unique_lock<std::mutex> lck(m_mux);
        handle.socket = sock;
        m_connection_map[handle.remote_addr] = sock;
        m_send_mutex_map[sock];
        m_buf_map[sock].pos = 0;
        m_buf_map[sock].left_to_read = 0;
        m_buf_map[sock].size = m_buffersize;
        m_buf_map[sock].buf_array.resize(m_buffersize, '\0');
        m_buf_map[sock].buf = m_buf_map[sock].buf_array.data();
    }
    if(m_on_connnection_established_cb != NULL){
        m_on_connnection_established_cb(connection_info{{"", 0},{inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port)}, -1, sock}, this, m_config.usr_ctx);
    }
    auto ctx = ev_context{sock, NULL, std::bind(&NetworkPosix::on_data, this, std::placeholders::_1, std::placeholders::_2)};
    m_evloop.register_fd(ctx);
}


ssize_t NetworkPosix::send_data(const char* data, size_t size, network_handle* handle){
    return std::visit(
        overloaded{[&](connection_info& handle) -> ssize_t { 
            if(m_connection_map.count(handle.remote_addr) > 0){
                char header[sizeof(size_t)];
                snprintf(header, sizeof(header), "%zu", size);
                std::unique_lock<std::mutex> lck(m_send_mutex_map[m_connection_map[handle.remote_addr]]); //Need to lock here to ensure message integrity in connection stream
                send(m_connection_map[handle.remote_addr], header, sizeof(size_t), 0);
                return send(m_connection_map[handle.remote_addr], data, size, 0);
            } else if (std::find_if(m_connection_map.begin(), m_connection_map.end(), [=](auto& connection){return connection.second == handle.socket;}) != m_connection_map.end()) {
                char header[sizeof(size_t)];
                snprintf(header, sizeof(header), "%zu", size);
                std::unique_lock<std::mutex> lck(m_send_mutex_map[handle.socket]);
                send(handle.socket, header, sizeof(size_t), 0);
                return send(handle.socket, data, size, 0);
            } else {
                try{
                    send_connect(handle);
                } catch (std::runtime_error& e){
                    throw;
                }
                char header[sizeof(size_t)];
                snprintf(header, sizeof(header), "%zu", 9);
                std::unique_lock<std::mutex> lck(m_send_mutex_map[handle.socket]);
                send(handle.socket, header, sizeof(size_t), 0);
                return send(handle.socket, data, size, 0);
            }
        },
        [](std::monostate& handle) -> ssize_t {
            throw std::invalid_argument("Network handle is not initialized");
        },
        [](auto& handle) -> ssize_t {
            throw std::invalid_argument("Wrong handle for this netwokr provider. Expecting TCP handle.");
        },
    }, *handle);
}
