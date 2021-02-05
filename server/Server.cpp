#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "Config.hpp"
#include "NetQuery.hpp"

void Server::run(bool edge_triggered)
{
  uint flags; // event flags for new conections
  if (edge_triggered) {
    flags = EPOLLIN | EPOLLET;  // if needed EPOLLOUT can be added here too
  } else {
    flags = EPOLLIN;  // EPOLLIN must be removed after reading to add EPOLLOUT
  }

  // main loop
  struct epoll_event ev, events[EVENTS_MAX];
  for (;;) {
    // wait for epoll events
    int nfds;
    if ((nfds = epoll_wait(epfd, events, EVENTS_MAX, -1)) == -1) {
      perror("epoll_wait()");
      exit(EXIT_FAILURE);
    }

    // handle events
    for (int i = 0; i < nfds; ++i) {
      ev = events[i];
      if (ev.data.fd == listenfd) {
        // new socket user detected, accept connection
        struct sockaddr_in clientaddr;
        socklen_t clilen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clilen);
        if (connfd == -1) {
          perror("accept()");
          exit(EXIT_FAILURE);
        }

        // new connection
        std::cout << "Accepting a connection (" << connfd << ") from " << inet_ntoa(clientaddr.sin_addr) << "\n";
        setNonBlocking(connfd);
        ev.data.fd = connfd;
        ev.events = flags;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
          perror("epoll_ctl()");
          exit(EXIT_FAILURE);
        }
      } else if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        // error
        perror("epoll event");
        close(ev.data.fd);
        continue;
      } else if (ev.events & EPOLLIN) {
        if (edge_triggered)
          handleET(ev.data.fd);
        else
          handleLT(ev.data.fd);
      } else {
        perror("unhandled epoll event");
        exit(EXIT_FAILURE);
      }
    }
  }
}

void Server::handleET(int fd)
{
  // read all data from socket until EAGAIN
  char line[LINE_SIZE];
  for (;;) {
    ssize_t n = read(fd, line, LINE_SIZE);
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // all data read
        break;
      } else if (errno == ECONNRESET) {
        close(fd);
      } else {
        perror("read()");
        exit(EXIT_FAILURE);
      }
    } else if (n == 0) {
      // client closed connection
      std::cout << "Client (" << fd << ") closed connection.\n";
      close(fd);
      break;
    } else {
      // write back read line
      write(fd, line, n);
    }
  }
}

void Server::handleLT(int fd)
{
  // data received, read it
  char line[LINE_SIZE];
  ssize_t n = read(fd, line, LINE_SIZE);
  if (n == -1) {
    if (errno == ECONNRESET) {
      close(fd);
    } else {
      perror("read()");
      exit(EXIT_FAILURE);
    }
  } else if (n == 0) {
    // client closed connection
    std::cout << "Client (" << fd << ") closed connection.\n";
    close(fd);
  } else {
    // write back received data (fails if message is longer than line size)
    write(fd, line, n);
  }
}

// init epoll and listening socket
void Server::init(uint16_t port)
{
  // epoll instance
  if ((epfd = epoll_create(256)) == -1) {
    perror("epoll_create()");
    exit(EXIT_FAILURE);
  }

  // create socket
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket()");
    exit(EXIT_FAILURE);
  }
  setNonBlocking(listenfd);

#ifndef NDEBUG
  // allow immediate address reuse on restart (debug only)
  int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
    perror("setsockopt()");
    exit(EXIT_FAILURE);
  }
#endif

  // register listening socket
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.data.fd = listenfd;
  ev.events = EPOLLIN;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
    perror("epoll_ctl()");
    exit(EXIT_FAILURE);
  }

  // bind and listen
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
    perror("bind()");
    exit(EXIT_FAILURE);
  }
  setNonBlocking(listenfd);
  if (listen(listenfd, LISTEN_QUEUE_SIZE) == -1) {
    perror("listen()");
    exit(EXIT_FAILURE);
  }
}

// set socket to non-blocking
void Server::setNonBlocking(int socket)
{
  int flags = fcntl(socket, F_GETFL);
  if (flags == -1) {
    perror("fcntl(GETFL)");
    exit(EXIT_FAILURE);
  }
  flags = flags | O_NONBLOCK;
  if (fcntl(socket, F_SETFL, flags) == -1) {
    perror("fcntl(SETFL)");
    exit(EXIT_FAILURE);
  }
}
