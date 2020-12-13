#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "Config.hpp"
#include "NetQuery.hpp"

void Server::run()
{
  // init epoll and listening socket
  init();

  // main loop
  struct epoll_event ev, events[EVENTS_MAX];
  for (;;) {
    // wait for epoll events
    int nfds;
    if ((nfds = epoll_wait(epfd, events, EVENTS_MAX, WAIT_TIMEOUT)) == -1) {
      perror("epoll_wait()");
      exit(EXIT_FAILURE);
    }

    // handle events
    for (int i = 0; i < nfds; ++i) {
      ev = events[i];
      if (ev.data.fd == listenfd) {
        // new socket user(s) detected, accept connection(s)
        for (;;) {
          struct sockaddr_in clientaddr;
          socklen_t clilen = sizeof(clientaddr);
          int connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clilen);
          if (connfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // all connections handled
              break;
            } else {
              perror("accept()");
              exit(EXIT_FAILURE);
            }
          } else {
            // new connection
            std::cout << "Accepting a connection (" << connfd << ") from " << inet_ntoa(clientaddr.sin_addr) << "\n";
            setNonBlocking(connfd);
            ev.data.fd = connfd;
            ev.events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
              perror("epoll_ctl()");
              exit(EXIT_FAILURE);
            }
          }
        }
      } else if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        // error
        perror("epoll event");
        close(ev.data.fd);
        continue;
      } else if (ev.events & EPOLLIN) {
        // data received, read it
        readFromSocket(ev.data.fd);

        //} else if (events[i].events & EPOLLOUT) { // write buffer empty again
        /* how to add listening for write buffer event:
         * ev.data.fd = sockfd;
         * ev.events = EPOLLOUT | EPOLLET;
         * epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
         */
      } else {
        perror("unhandled epoll event");
        exit(EXIT_FAILURE);
      }
    }
  }
}

// init epoll and listening socket
void Server::init()
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

#ifdef DEBUG
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
  ev.events = EPOLLIN | EPOLLET;  // edge triggered
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
    perror("epoll_ctl()");
    exit(EXIT_FAILURE);
  }

  // bind and listen
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  serveraddr.sin_port = htons(Config::serverPort);
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

// read data after receiving EPOLLIN
inline void Server::readFromSocket(int socket)
{
  // struct including fd and read data to be used in a load balancer for worker threads
  NetQuery* query = new NetQuery(socket);

  // read all data from socket (socket buffer can be bigger than LINE_SIZE)
  char line[LINE_SIZE];
  for (;;) {
    ssize_t n = read(socket, line, LINE_SIZE);
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // all data read
        break;
      } else if (errno == ECONNRESET) {
        close(socket);
      } else {
        perror("read()");
        exit(EXIT_FAILURE);
      }
    } else if (n == 0) {
      // client closed connection
      std::cout << "Client (" << socket << ") closed connection." << std::endl;
      close(socket);
      break;
    } else {
      // add data to query object
      query->data.append(line, n);
    }

    // write back received data
    write(socket, query->data.c_str(), query->data.length() + 1);
  }
}