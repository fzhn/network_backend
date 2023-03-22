#include "eventloop.h"
#include <algorithm>

Eventloop::Eventloop(std::function<void()> cb_init) : m_epollfd(epoll_create1(0)),
                                                      cb_init(cb_init)
{
    if(m_epollfd == -1) {
      log_fatal("Could not create epoll fd. Exit.");
      exit(2);
    }
}


void Eventloop::process_event(struct ev_context* evc)
{
  if(evc->cb != NULL) {
      evc->cb(evc->fd, evc->data);
  }
}

void Eventloop::stop(){
  ev_should_stop = true;
}


void Eventloop::register_fd(ev_context& ctx){
    struct epoll_event ev;
    int ev_fd = ctx.fd;
    m_polled_fids[ev_fd] = ctx;
    ev.events = (EPOLLIN | EPOLLRDHUP);
    ev.data.ptr = &m_polled_fids[ev_fd];
    int rc = fcntl(ev_fd, F_SETFL, fcntl(ev_fd, F_GETFL) | O_NONBLOCK );
    if (rc < 0) {
      log_error("Failed to change flags (incl. O_NONBLOCK) of file descriptor %d.", m_epollfd);
    }
    log_debug("Adding %d to epoll %d", ev_fd, m_epollfd);
    if(epoll_ctl(m_epollfd, EPOLL_CTL_ADD, ev_fd, &ev))
    {
        log_error("Could not add file descriptor %d to epoll. Events from this resource will be neglected.", m_epollfd);
        return;
    }
    m_open_fds.push_back(ctx.fd);
}

void Eventloop::remove_fd(int fd){
  if (std::find(m_open_fds.begin(), m_open_fds.end(), fd) != m_open_fds.end()){
    close(fd);
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, NULL);
    m_polled_fids.erase(fd);
    m_open_fds.erase(std::remove(m_open_fds.begin(), m_open_fds.end(), fd), m_open_fds.end());
  } else {
    log_warn("Tried to close FD %d which is not open. Doing nothing.", fd);
  }

}

void Eventloop::evloop_run(){
  int nevents;

  if(cb_init != NULL) {
      cb_init();
  }
  bool running = true;
  while(running) {
    // don't want to block or wait too long if we're shutting down
    uint64_t timeout = ev_should_stop ? 10 : EPOLL_TIMEOUT;
    nevents = epoll_wait(m_epollfd, &m_events[0], MAX_EPOLL_EVENTS, timeout);
    log_debug("epoll wait: %d events to process", nevents);

    for(int i = 0; i < nevents; ++i)
    {
      log_debug("event type: %x from fd %d", m_events[i].events, m_events[i].data.fd);
      process_event((ev_context*)(m_events[i].data.ptr));
      if(m_events[i].events & EPOLLRDHUP)
      {
        ev_context* c = (struct ev_context*)(m_events[i].data.ptr);
        if (c->on_closed_cb != NULL){
          c->on_closed_cb(c->fd, c->data);
        }
        remove_fd(c->fd);
      }
    }
    if (ev_should_stop && nevents==0) {
       running = false;
    }

    if(nevents == -1)
    {
      int errsv = errno;
      if(errsv==EINTR) {
        continue;
      }
      else {
        log_fatal("Eventloop: non-blocking epoll_wait returned -1: %s", strerror(errsv));
        exit(1);
      }
    }
  }//end of while running
  
  close(m_epollfd);
  std::for_each(m_open_fds.begin(), m_open_fds.end(), [](auto& fd){close(fd);});
}


