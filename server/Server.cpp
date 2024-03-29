#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

Server::Connection::Connection(int fd) : fd(fd) {}

/*void Server::Connection::MessageHandler::operator()(std::vector<uint8_t>& data) const
{
  eventCounter++;
}*/

Server::Server()
{
  pthread_rwlock_init(&connLatch, NULL);
}

Server::~Server()
{
  pthread_rwlock_destroy(&connLatch);
}

void Server::run(int threadCount)
{
  for (int t_i = 0; t_i < threadCount; t_i++) {
    threads.emplace_back(&Server::runThread, this);
  }

  // benchmark
  for (;;) {
    auto startTime = std::chrono::high_resolution_clock::now();
    TPCC::eventCounter = 0;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    uint64_t events = TPCC::eventCounter;
    std::chrono::duration<double, std::milli> mSec = std::chrono::high_resolution_clock::now() - startTime;
    std::cout << events << " " << mSec.count() << " " << events * 1000 / mSec.count() << "\n";
  }
}

void Server::runThread()
{
  // main loop
  struct epoll_event ev, events[1];
  Connection* connection;

  for (;;) {
    // wait for epoll events
    int nfds;
    if ((nfds = epoll_wait(epfd, events, 1, -1)) == -1) {
      perror("epoll_wait()");
      exit(EXIT_FAILURE);
    }

    // event loop
    for (int i = 0; i < nfds; ++i) {
      ev = events[i];
      if (ev.data.fd == listenfd) {
        // new socket user detected, accept connection
        struct sockaddr_in clientaddr;
        socklen_t clilen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clilen);
        if (connfd == -1) {
          if (errno = EAGAIN || errno == EWOULDBLOCK)
            continue;

          perror("accept()");
          exit(EXIT_FAILURE);
        }

        // add connection to hashtable and epoll
        pthread_rwlock_wrlock(&connLatch);
        connections.try_emplace(connfd, connfd);
        pthread_rwlock_unlock(&connLatch);

        setNonBlocking(connfd);
        ev.data.fd = connfd;
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
          perror("epoll_ctl()");
          exit(EXIT_FAILURE);
        }
      } else if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        // client closed connection
        closeConnection(ev.data.fd);
      } else {
        if (ev.events & EPOLLOUT) {
          // write from outBuffer
          pthread_rwlock_rdlock(&connLatch);
          connection = &connections.at(ev.data.fd);
          pthread_rwlock_unlock(&connLatch);
          auto& buf = connection->outBuffer;

          for (;;) {
            auto n = send(ev.data.fd, &buf[0], buf.size(), MSG_NOSIGNAL);
            if (n == -1) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket buffer full
                break;
              } else if (errno == ECONNRESET) {
                // client closed connection
                closeConnection(ev.data.fd);
                // continue with the event loop
                goto continue_event_loop;
              } else {
                perror("send()");
                exit(EXIT_FAILURE);
              }
            } else if (n != buf.size()) {
              // there's still some of the message left
              buf.erase(buf.begin(), buf.begin() + n);
              connection->epollEvents |= EPOLLOUT;
            } else {
              // complete message has been sent
              buf.clear();
              connection->epollEvents &= ~EPOLLOUT;
              break;
            }
          }
        }

        if (ev.events & EPOLLIN) {
          pthread_rwlock_rdlock(&connLatch);
          connection = &connections.at(ev.data.fd);
          pthread_rwlock_unlock(&connLatch);

          // read all data from socket until EAGAIN
          char buf[bufferSize];
          for (;;) {
            ssize_t n = read(ev.data.fd, buf, bufferSize);
            if (n == -1) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // all data read
                break;
              } else if (errno == ECONNRESET) {
                // client closed connection
                closeConnection(ev.data.fd);
                // continue with the event loop
                goto continue_event_loop;
              } else {
                perror("read()");
                exit(EXIT_FAILURE);
              }
            } else if (n == 0) {
              // client closed connection
              closeConnection(ev.data.fd);
              // continue with the event loop
              goto continue_event_loop;
            } else {
              // forward buf to packet protocol handler
              //              connection->packetizer.receive(reinterpret_cast<const uint8_t*>(buf), n);
              connection->parser.parse(reinterpret_cast<uint8_t*>(buf), n);
            }
          }
        }

        // rearm socket
        ev.events = connection->epollEvents;
        if (epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1) {
          perror("epoll_ctl()");
          exit(EXIT_FAILURE);
        }
      }

    continue_event_loop:;
    }
  }
}

// init epoll and listening socket
void Server::init(uint16_t port, size_t bufferSize)
{
  this->bufferSize = bufferSize;

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

  // allow immediate address reuse on restart
  // NOTE: tcp pakets from a previous run could still be pending
  int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
    perror("setsockopt()");
    exit(EXIT_FAILURE);
  }

  // register listening socket
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.data.fd = listenfd;
  ev.events = EPOLLIN | EPOLLEXCLUSIVE;

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

void Server::closeConnection(int fd)
{
  pthread_rwlock_wrlock(&connLatch);
  connections.erase(fd);
  pthread_rwlock_unlock(&connLatch);
  close(fd);
}
