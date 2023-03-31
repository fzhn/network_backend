#pragma once

//#include <rdma/fabric.h>
#include <iostream>
#include <array>
#include <map>
#include <sys/epoll.h>
#include <rdma/fi_domain.h>
#include <fcntl.h>
#include <functional>
#include <mutex>

#include "networkUtilities.h"


#define MAX_EPOLL_EVENTS (128)
#define EPOLL_TIMEOUT (1000)

class Eventloop;

struct ev_context
{
    int fd;
    void* data;
    std::function<void(int,void*)> cb;
    std::function<void(int,void*)> on_closed_cb;
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
    std::mutex m_fd_mutex;

    void process_event(struct ev_context* evc);
  public:
    Eventloop(std::function<void()> cb_init = NULL);
    void evloop_run();
    void register_fd(ev_context& ctx); //rename function
    void remove_fd(int fd);
    void stop();
};

class EvSignal{
  private:
    void* m_data;
    std::function<void(int,void*)> m_cb;
    Eventloop* m_evloop;
    ev_context m_ev_ctx;
    void exec_once(int,void*){
        m_cb(m_ev_ctx.fd, m_data);
        m_evloop->remove_fd(m_ev_ctx.fd);
    }
    void exec(int,void*){
        m_cb(m_ev_ctx.fd, m_data);
    }
  public:  
    EvSignal(void* data, std::function<void(int,void*)> cb, Eventloop* evloop, bool use_semaphore, bool exec_once) :    m_data(data),
                                                                                                                        m_cb(cb),
                                                                                                                        m_evloop(evloop),
                                                                                                                        m_ev_ctx(ev_context())
    {
        if(use_semaphore){
            m_ev_ctx.fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
        } else {
            m_ev_ctx.fd = eventfd(0, EFD_NONBLOCK);
        }
        if (exec_once){
            m_ev_ctx.cb =  std::bind(&EvSignal::exec_once, this, std::placeholders::_1, std::placeholders::_2);
        } else {
            m_ev_ctx.cb =  std::bind(&EvSignal::exec, this, std::placeholders::_1, std::placeholders::_2);
        }

        m_ev_ctx.data = this;
        if(m_ev_ctx.fd == -1)
        {
            log_fatal("Could not open eventfd");
            exit(2);
        }
        m_evloop->register_fd(m_ev_ctx);
    }

    ~EvSignal(){
        m_evloop->remove_fd(m_ev_ctx.fd);
    }

    void fire(){
        uint64_t buf = 1;
        int ret = write(m_ev_ctx.fd, &buf, 8);
        if( ret !=8 ){
            log_error("Firing signal writing on fd %d, only %d / 8 bytes written. Errno %s", m_ev_ctx.fd, ret, strerror(errno));
        }
    }

    void set_data(void* data){
        m_data = data;
    }
};