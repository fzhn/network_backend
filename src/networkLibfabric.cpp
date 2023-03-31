#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <arpa/inet.h>

#include "network.h"

#define MAX_CQ_ENTRIES (4096)

struct endpoint{
    struct fi_info* fi;
    struct fid_eq *eq = NULL;
    struct fid_ep *ep = NULL;
    struct fid_cq* cq = NULL;
    struct fid_cq* rcq;
    int eqfd = -1;
    int cqfd = -1;
};


struct listen_socket{
    endpoint ep;
    struct fid_pep *pep;

    int fi_errno;
    char* fi_message;
    ev_context eq_ev_ctx;
};


struct send_socket{
    endpoint ep;
    ev_context eq_ev_ctx;
};


int NetworkLibfabric::read_cm_event(struct fid_eq* eq, struct fi_info** info, struct fi_eq_err_entry* err_entry){
    uint32_t event;
    struct fi_eq_cm_entry entry;

    ssize_t rd = fi_eq_sread(eq, &event, &entry, sizeof entry, 0, 0);
    if(rd < 0)
    {
        if(rd == -FI_EAGAIN)
        {
            return rd;
        }
        if(rd == -FI_EAVAIL)
        {
            int r;
            if((r = fi_eq_readerr(eq, err_entry, 0)) < 0)
            {
                log_error("Failed to retrieve details on Event Queue error", r);
            }
            log_error("Event Queue error: %s (code: %d), provider specific: %s (code: %d)",
                fi_strerror(err_entry->err), err_entry->err,
                fi_eq_strerror(eq, err_entry->prov_errno, err_entry->err_data, NULL, 0),
                err_entry->prov_errno);
            return rd;
        }
    }
    if (rd != sizeof entry)
    {
        log_error("Failed to read from Event Queue: %d", (int)rd);
    }

    if(info != NULL)
        *info = entry.info;

    return event;
}

void NetworkLibfabric::handle_connreq()
{


    // if((ret = fi_accept(rsocket->ep, NULL, 0)))
    // {
    //     fi_reject(lsocket->pep, info->handle, NULL, 0);
    //     FATAL("Listen socket, connection rejected, error ", ret);;
    // }
    // log_dbg("connection accepted. Lsocket EQ: %d with evloop %d, rsocket EQ %d with evloop %d", lsocket->eqfd, lsocket->ctx->evloop.epollfd, rsocket->eqfd, rsocket->ctx->evloop.epollfd);
}


void NetworkLibfabric::on_listen_socket_cm_event(int fd,void* ptr){
    listen_socket* lsocket = (listen_socket*)ptr;
    log_debug("listen socket: connection event");

    struct fi_info *info = NULL;
    struct fi_eq_err_entry err_entry;
    int event = read_cm_event(lsocket->ep.eq, &info, &err_entry);

    switch (event)
    {
        case FI_CONNREQ:
            log_debug("fi_verbs_process_listen_socket_cm_event: FI_CONNREQ");
            // struct netio_socket_list* new_entry = add_socket(&lsocket->recv_sockets, URECV);
            // struct netio_recv_socket* rsocket = (struct netio_recv_socket*)new_entry->socket;
            // netio_init_recv_socket(rsocket, lsocket);
            // handle_connreq(rsocket, lsocket, info, on_recv_socket_cm_event, rsocket);
            // if(lsocket->recv_sub_msg == 1){
            //     rsocket->sub_msg_buffers = malloc(32*sizeof(struct netio_buffer*));
            //     for (int i = 0; i < 32; i++){
            //         rsocket->sub_msg_buffers[i] = malloc(sizeof(struct netio_buffer));
            //         rsocket->sub_msg_buffers[i]->size = sizeof(struct netio_subscription_message);
            //         rsocket->sub_msg_buffers[i]->data = malloc(rsocket->sub_msg_buffers[i]->size);
            //         netio_register_recv_buffer(rsocket, rsocket->sub_msg_buffers[i], 0);
            //         netio_post_recv(rsocket, rsocket->sub_msg_buffers[i]);
            //     }
            //     log_dbg("Posted recv for subscription messages");
            // } else if (lsocket->attr.num_buffers > 0) {
            //     log_dbg("Connection established, posting %lu buffers", lsocket->attr.num_buffers);
            //     rsocket->sub_msg_buffers = NULL;
            //     rsocket->recv_buffers = malloc(lsocket->attr.num_buffers * sizeof(struct netio_buffer));
            //     for(unsigned i=0; i<lsocket->attr.num_buffers; i++) {
            //         log_trc("registering buffer of size %lu", lsocket->attr.buffer_size);
            //         rsocket->recv_buffers[i].size = lsocket->attr.buffer_size;
            //         rsocket->recv_buffers[i].data = malloc(lsocket->attr.buffer_size);
            //         netio_register_recv_buffer(rsocket, &rsocket->recv_buffers[i], 0);
            //         netio_post_recv(rsocket, &rsocket->recv_buffers[i]);
            //     }
            // } else {
            //     log_error("Something went wrong. Not allocating any buffers for recv socket.");
            // }
            break;

        case FI_CONNECTED:
            log_fatal("FI_CONNECTED received on listen socket");
            exit(2);

        case FI_SHUTDOWN:
            log_fatal("FI_SHUTDOWN received on listen socket");
            exit(2);

        case -FI_EAGAIN:
            {
                struct fid* fp = &lsocket->ep.eq->fid;
                fi_trywait(&m_fabric, &fp, 1);
                break;
            }

        case -FI_EAVAIL:
            log_error("Unhandled error in listen socket EQ code: %d, provider specific code: %d",
                err_entry.err, err_entry.prov_errno);
            break;
    }
    fi_freeinfo(info);
}


void NetworkLibfabric::on_send_socket_cm_event(int, void*){

}

NetworkLibfabric::NetworkLibfabric(network_config& config) : Network(config),
                                                             m_evloop(Eventloop(NULL)),
                                                             m_send_domain(domain_ctx()),
                                                             m_info(fi_allocinfo())
{
    if (m_config.mode != RDMA){
        throw std::invalid_argument("Libfarbic backend only supports RDMA");
    }
    fi_info hints;   
    hints.addr_format = FI_FORMAT_UNSPEC;
    hints.ep_attr->type  = FI_EP_MSG;
    hints.caps = FI_MSG;
    hints.mode   = FI_LOCAL_MR;
    int ret = 0;

    if((ret = fi_getinfo(FI_VERSION(1, 1), NULL, NULL, FI_SOURCE, &hints, &m_info)))
    {
        log_error("Failed to get info on local interface, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_getinfo failed", ret);
        return;
    }
    log_debug("addr format: %x", m_info->addr_format);

    auto fabric_ptr = &m_fabric;
    if((ret = fi_fabric(m_info->fabric_attr, &fabric_ptr, NULL)))
    {
        log_error("Failed to open fabric for listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_fabric failed", ret);
        return;
    }


}

NetworkLibfabric::~NetworkLibfabric(){
    m_evloop.stop();
}
void NetworkLibfabric::run(std::string name){

    m_evloop.evloop_run();
}
void NetworkLibfabric::register_handle(network_handle* handle){
    std::visit(
    overloaded{[&](connection_info& handle) {
            if(handle.local_addr.ip != ""){
                if(m_lsockets.count(handle.local_addr) > 0 ){
                    log_info("Listen socket for local add %s:%u is already exists.", handle.local_addr.ip, handle.local_addr.port);
                } else {
                    m_lsockets[handle.local_addr] = listen_socket();
                    init_listen(&m_lsockets[handle.local_addr], &handle);
                }
            } else if (handle.remote_addr.ip != ""){
                if(m_ssockets.count(handle.remote_addr) > 0 ){
                    log_info("Listen socket for local add %s:%u is already exists.", handle.remote_addr.ip, handle.remote_addr.port);
                } else {
                    m_ssockets[handle.remote_addr] = send_socket();
                    init_send(&m_ssockets[handle.remote_addr], &handle);
                }
            }

        },
        [](std::monostate& handle) {
            log_error("Empty network handle.");
            return;
        },
        [](auto& handle) {
            log_error("Wrong handle forthis netowkr provider. Using TCP.");
        },
    }, *handle);
}
void NetworkLibfabric::close_handle(network_handle* handle){

}
ssize_t NetworkLibfabric::send_data(const char* data, size_t size, network_handle* handle){return 0;}


int NetworkLibfabric::connect_endpoint(endpoint* ep, fid_domain* domain){
    int ret=0;
    fi_eq_attr eq_attr;
    eq_attr.wait_obj = FI_WAIT_FD;

    if((ret = fi_eq_open(&m_fabric, &eq_attr, &ep->eq, NULL)))
    {
        log_error("Failed to open Event Queue for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    if((ret = fi_endpoint(domain, ep->fi, &ep->ep, NULL)))
    {
        log_error("Failed to open Endpoint for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    if((ret = fi_ep_bind(ep->ep, &ep->eq->fid, 0)))
    {
        log_error("Failed to bind endpoint, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    fi_cq_attr cq_attr;
    cq_attr.size = MAX_CQ_ENTRIES;      /* # entries for CQ */
    cq_attr.flags = 0;     /* operation flags */
    cq_attr.format = FI_CQ_FORMAT_DATA; //FI_CQ_FORMAT_CONTEXT;    /* completion format */
    cq_attr.wait_obj= FI_WAIT_FD;  /* requested wait object */
    cq_attr.signaling_vector = 0; /* interrupt affinity */
    cq_attr.wait_cond = FI_CQ_COND_NONE; /* wait condition format */ // The threshold indicates the number of entries that are to be queued before at the CQ before the wait is satisfied.
    cq_attr.wait_set = NULL;  /* optional wait set */

    //FI_TRANSMIT CQ
    if((ret = fi_cq_open(domain, &cq_attr, &ep->cq, NULL)) != 0)
    {
        log_error("Failed to open Completion Queue for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    if((ret = fi_ep_bind((ep->ep), &ep->cq->fid, FI_TRANSMIT)) != 0)
    {
        log_error("Failed to open Completion Queue for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    //FI_RECV CQ - also necessary
    cq_attr.format = FI_CQ_FORMAT_UNSPEC;
    cq_attr.wait_obj= FI_WAIT_NONE;
    if((ret = fi_cq_open(domain, &cq_attr, &ep->rcq, NULL)) != 0)
    {
        log_error("Failed to open Completion Queue for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    if((ret = fi_ep_bind((ep->ep), &ep->rcq->fid, FI_RECV)) != 0)
    {
        log_error("Failed to open Completion Queue for send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }


    if((ret = fi_enable(ep->ep)) != 0)
    {
        log_error("Failed to enable endpoint for send socket, error %d: %s", ret, fi_strerror(-ret));;
        return -1;
    }

    if((ret = fi_control(&ep->eq->fid, FI_GETWAIT, &ep->eqfd)) != 0)
    {
        log_error("Cannot retrieve the Event Queue wait object of send socket, error %d: %s", ret, fi_strerror(-ret));
        return -1;
    }

    return ret;
}



void NetworkLibfabric::init_send(send_socket* socket, connection_info* connection){
    int ret=0;
    struct fi_info* hints;
    
    // Init infos

    hints = fi_allocinfo();
    hints->addr_format = FI_FORMAT_UNSPEC;
    hints->ep_attr->type  = FI_EP_MSG;
    hints->caps = FI_MSG;
    hints->mode   = FI_LOCAL_MR;
    // As of libfabric 1.10, the tcp provider only support FI_PROGRESS_MANUAL
    // So the following will not allow the tcp provider to be used
    hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
    hints->domain_attr->resource_mgmt = FI_RM_ENABLED;

    uint64_t flags = 0;
    if((ret = fi_getinfo(FI_VERSION(1, 1), connection->local_addr.ip.c_str(), std::to_string(htons(connection->remote_addr.port)).c_str(), flags, hints, &socket->ep.fi)))
    {
        fi_freeinfo(hints);
        log_error("Failed to initialise socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_CONNECTION_REFUSED(socket, "fi_getinfo failed", ret);
        return;
    }
    fi_freeinfo(hints);

    // Init domain
    m_send_domain.nb_sockets++;
    if(m_send_domain.nb_sockets > 1){
        auto fabric_ptr = &m_fabric;
        if((ret = fi_fabric(socket->ep.fi->fabric_attr, &fabric_ptr, NULL)))
        {
            log_error("Failed to initialise fabric, error %d: %s", ret, fi_strerror(-ret));
            return;
        }
        auto domain_ptr = &m_send_domain.domain;
        if((ret = fi_domain(&m_send_domain.fabric, socket->ep.fi, &domain_ptr, NULL)))
        {
            log_error("Failed to initialise domain, error %d: %s", ret, fi_strerror(-ret));
            return;
        }
    }

    //Connect socket
    if(connect_endpoint(&socket->ep, &m_send_domain.domain)){
        //do somehting to try again
    }
    /* Connect to server */
    if((ret = fi_connect(socket->ep.ep, socket->ep.fi->dest_addr, NULL, 0)) != 0)
    {
        log_warn("Connection to remote failed, error %d: %s", ret, fi_strerror(-ret));
        return;
    }  
    if((ret = fi_control(&socket->ep.eq->fid, FI_GETWAIT, &socket->ep.eqfd)))
    socket->eq_ev_ctx = ev_context{socket->ep.eqfd, socket, std::bind(&NetworkLibfabric::on_send_socket_cm_event, this, std::placeholders::_1, std::placeholders::_2)};
    m_evloop.register_fd(socket->eq_ev_ctx);
    return;
}


void NetworkLibfabric::init_listen(listen_socket* socket, connection_info* connection){
    int ret=0;
    struct fi_info* hints;
    struct fi_eq_attr eq_attr;
    eq_attr.wait_obj = FI_WAIT_FD;

    hints = fi_allocinfo();
    hints->addr_format = FI_FORMAT_UNSPEC;
    hints->ep_attr->type  = FI_EP_MSG;
    hints->caps = FI_MSG;
    hints->mode = FI_LOCAL_MR;

    // //Resource initialisation
    // socket->eqfd = -1;
    // socket->pep = NULL;
    // socket->eq = NULL;
    // socket->fi = NULL;

    if((ret = fi_getinfo(FI_VERSION(1, 1), connection->local_addr.ip.c_str(), std::to_string(connection->local_addr.port).c_str(), FI_SOURCE, hints,
                        &socket->ep.fi)))
    {
        log_error("Failed to get info on local interface, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_getinfo failed", ret);
        return;
    }
    log_debug("addr format: %x", socket->ep.fi->addr_format);
    auto fabric_ptr = &m_fabric;
    if((ret = fi_fabric(socket->ep.fi->fabric_attr, &fabric_ptr, NULL)))
    {
        log_error("Failed to open fabric for listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_fabric failed", ret);
        return;
    }
    if((ret = fi_eq_open(&m_fabric, &eq_attr, &socket->ep.eq, NULL)))
    {
        log_error("Failed to open Event Queue for listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_eq_open failed", ret);
        return;
    }

    if((ret = fi_passive_ep(&m_fabric, socket->ep.fi, &socket->pep, NULL)))
    {
        log_error("Failed to open passive endpoint for listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_passive_ep failed", ret);
        return;
    }

    if((ret = fi_pep_bind(socket->pep, &socket->ep.eq->fid, 0)))
    {
        log_error("Failed to bind passive endpoint for listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_pep_bind failed", ret);
        return;
    }

    if((ret = fi_listen(socket->pep)))
    {
        log_error("Failed to enable listen socket, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_listen failed", ret);
        return;
    }

    if((ret = fi_control(&socket->ep.eq->fid, FI_GETWAIT, &socket->ep.eqfd)))
    {
        log_error("Failed to retrive listen socket Event Queue wait object, error %d: %s", ret, fi_strerror(-ret));
        //ON_ERROR_BIND_REFUSED(socket, "fi_control failed", ret);
        return;
    }

    socket->eq_ev_ctx = ev_context{socket->ep.eqfd, socket, std::bind(&NetworkLibfabric::on_listen_socket_cm_event, this, std::placeholders::_1, std::placeholders::_2)};

    m_evloop.register_fd(socket->eq_ev_ctx);
    log_debug("netio_listen_socket: registering EQ fd %d", socket->ep.eqfd);
    fi_freeinfo(hints);
}

