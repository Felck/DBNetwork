#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

Server::Connection::Connection(int fd) : fd(fd), packetizer(MessageHandler{*this}) {
}

void Server::Connection::MessageHandler::operator()(std::vector<uint8_t>& data) const
{
  // write back msg
  auto msg = Net::wrapMessage(&data[0], data.size());

  if (connection.outBuffer.empty()) {
    auto n = write(connection.fd, &msg[0], msg.size());
    if (n != msg.size()) {
      // buffer remaining part of msg
      auto& buf = connection.outBuffer;
      buf.insert(buf.begin(), msg.begin() + n, msg.end());
      connection.epollEvents |= EPOLLOUT;
    }
  } else {
    // buffer msg
    connection.outBuffer.insert(connection.outBuffer.end(), msg.begin(), msg.end());
  }
}

Server::Server()
{
  pthread_rwlock_init(&connLatch, NULL);
}

Server::~Server()
{
  pthread_rwlock_destroy(&connLatch);
}

void Server::run(int threadCount, size_t lineSize)
{
  for (int t_i = 0; t_i < threadCount; t_i++) {
    threads.emplace_back([&]() {
      // main loop
      struct epoll_event ev, events[1];
      Connection* connection;
      bool rearmSocket = false;

      for (;;) {
        // wait for epoll events
        int nfds;
        if ((nfds = epoll_wait(epfd, events, 1, -1)) == -1) {
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
              if (errno = EAGAIN || errno == EWOULDBLOCK)
                continue;

              perror("accept()");
              exit(EXIT_FAILURE);
            }

            // std::cout << "Accepting a connection (" << connfd << ") from " << inet_ntoa(clientaddr.sin_addr) << "\n";

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
            // error
            perror("Closing connection");
            closeConnection(ev.data.fd);
            continue;
          } else if (ev.events & EPOLLIN) {
            pthread_rwlock_rdlock(&connLatch);
            connection = &connections.at(ev.data.fd);
            pthread_rwlock_unlock(&connLatch);

            // read all data from socket until EAGAIN
            char line[lineSize];
            for (;;) {
              ssize_t n = read(ev.data.fd, line, lineSize);
              if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                  // all data read
                  rearmSocket = true;
                } else if (errno == ECONNRESET) {
                  // client closed connection
                  // std::cout << "Client (" << ev.data.fd << ") closed connection.\n";
                  closeConnection(ev.data.fd);
                  // continue with the event loop
                  goto continue_outer;
                } else {
                  perror("read()");
                  exit(EXIT_FAILURE);
                }
                break;
              } else if (n == 0) {
                // client closed connection
                // std::cout << "Client (" << ev.data.fd << ") closed connection.\n";
                closeConnection(ev.data.fd);
                // continue with the event loop
                goto continue_outer;
              } else {
                // forward line to packet protocol handler
                connection->packetizer.receive(reinterpret_cast<const uint8_t*>(line), n);
              }
            }
          } else if (ev.events & EPOLLOUT) {
            // lookup the sockets pending writeTask and continue with that
            pthread_rwlock_rdlock(&connLatch);
            connection = &connections.at(ev.data.fd);
            pthread_rwlock_unlock(&connLatch);
            auto& buf = connection->outBuffer;

            auto n = write(ev.data.fd, &buf[0], buf.size());
            if (n != buf.size()) {
              // there's still some of the message left
              buf.erase(buf.begin(), buf.begin() + n);
              connection->epollEvents |= EPOLLOUT;
            } else {
              // complete message has been sent
              buf.clear();
              connection->epollEvents &= ~EPOLLOUT;
            }
            // rearm socket
            rearmSocket = true;
          } else {
            std::cout << ev.events << std::endl;
            perror("unhandled epoll event");
          }

          if (rearmSocket) {
            rearmSocket = false;
            ev.events = connection->epollEvents;
            if (epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1) {
              perror("epoll_ctl()");
              exit(EXIT_FAILURE);
            }
          }
        continue_outer:;
        }
      }
    });
  }
  // TODO
  threads[0].join();
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

  // allow immediate address reuse on restart
  /** FIXME:
   * Packets from the previous run could still be received and need to be ignored.
   * The packet framing protocol might be the best choice for this.
   */
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
