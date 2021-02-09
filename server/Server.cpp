#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

void Server::MessageHandler::operator()(std::vector<uint8_t>& data) const
{
  //std::cout << "recv(" << fd << "): " << std::string(data.begin(), data.end()) << "\n";
  // write back msg
  auto msg = Net::wrapMessage(&data[0], data.size());
  auto n = write(fd, &msg[0], msg.size());

  // add task for later EPOLLOUT events if not all could be written
  if (n != msg.size())
  {
    // TODO: synchronize
    auto& task = writeTasks[fd];
    task.insert(task.begin(), msg.begin() + n, msg.end());
  }
}

void Server::run()
{
  EPOLLIN | EPOLLOUT | EPOLLET;

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

        std::cout << "Accepting a connection (" << connfd << ") from " << inet_ntoa(clientaddr.sin_addr) << "\n";

        // add connection to hashtable and epoll
        // TODO: synchronization
        connections.emplace(connfd, MessageHandler{connfd, writeTasks});

        setNonBlocking(connfd);
        ev.data.fd = connfd;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // TODO: add EPOLLONESHOT when going multithreaded
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
          perror("epoll_ctl()");
          exit(EXIT_FAILURE);
        }
      } else if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        // error
        perror("Closing connection");
        connections.erase(ev.data.fd);
        close(ev.data.fd);
        continue;
      } else if (ev.events & EPOLLIN) {
        // read all data from socket until EAGAIN
        char line[LINE_SIZE];
        for (;;) {
          ssize_t n = read(ev.data.fd, line, LINE_SIZE);
          if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // all data read
              break;
            } else if (errno == ECONNRESET) {
              close(ev.data.fd);
            } else {
              perror("read()");
              exit(EXIT_FAILURE);
            }
          } else if (n == 0) {
            // client closed connection
            std::cout << "Client (" << ev.data.fd << ") closed connection.\n";
            connections.erase(ev.data.fd);
            close(ev.data.fd);
            break;
          } else {
            // forward line to protocol handler
            connections.at(ev.data.fd).receive(reinterpret_cast<const uint8_t*>(line), n);
          }
        }
      } else if (ev.events & EPOLLOUT) {
        // TODO: synchronize
        // lookup the sockets pending writeTask and continue with that
        auto it = writeTasks.find(ev.data.fd);
        if (it != writeTasks.end()) { // <- should be unnecessary with EPOLLONESHOT
          auto& msg = it->second;
          auto n = write(ev.data.fd, &msg[0], msg.size());
          if (n != msg.size()) {
            msg.erase(msg.begin(), msg.begin() + n);
          } else {
            writeTasks.erase(it);
          }
        }
      } else {
        std::cout << ev.events << std::endl;
        perror("unhandled epoll event");
      }
    }
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
