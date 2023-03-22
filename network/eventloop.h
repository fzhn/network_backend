#pragma once

//#include <rdma/fabric.h>
#include <iostream>
#include <array>
#include <map>
#include <sys/epoll.h>
#include <rdma/fi_domain.h>
#include <fcntl.h>
#include <functional>

#include "networkUtilities.h"


#define MAX_EPOLL_EVENTS (128)
#define EPOLL_TIMEOUT (1000)

struct ev_context
{
    int fd;
    void* data;
    std::function<void(int,void*)> cb;
    std::function<void(int,void*)> on_closed_cb;
};

class EvSignal{
  private:
    void* data;
    std::function<void(int,void*)> cb;
    ev_context ev_ctx;
  public:  
    EvSignal(void* data, int fd, std::function<void(int,void*)> cb, bool use_semaphore) :   data(data),
                                                    cb(cb),
                                                    ev_ctx(ev_context())
    {
        if(use_semaphore){
            ev_ctx.fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
        } else {
            ev_ctx.fd = eventfd(0, EFD_NONBLOCK);
        }

        ev_ctx.data = this;
        if(ev_ctx.fd == -1)
        {
            log_fatal("Could not open eventfd");
            exit(2);
        }
    }

    ~EvSignal(){
    }

    void fire(){
        uint64_t buf = 1;
        int ret = write(ev_ctx.fd, &buf, 8);
        if( ret !=8 ){
            log_error("Firing signal writing on fd %d, only %d / 8 bytes written. Errno %s", ev_ctx.fd, ret, strerror(errno));
        }
    }
};


class EvTimer{
    EvTimer(){}
    ~EvTimer(){}
    void start_s(uint64_t t){start_us(t * 1000 * 1000);}
    void start_ms(uint64_t t){start_us(t * 1000);}
    void start_us(uint64_t t){}
    void stop(){}
};

class Eventloop {
  private:
    std::map<int, ev_context> m_polled_fids;
    int m_epollfd; // no flag passed, same behaviour as epoll_create
    std::array<struct epoll_event, MAX_EPOLL_EVENTS> m_events;
    std::function<void()> cb_init = NULL;
    std::vector<int> m_open_fds;
    std::vector<ev_context> m_ev_contexts;
    bool ev_should_stop = false;

    void process_event(struct ev_context* evc);
  public:
    Eventloop(std::function<void()> cb_init = NULL);
    void evloop_run();
    void register_fd(ev_context& ctx);
    void remove_fd(int fd);
    void stop();
};