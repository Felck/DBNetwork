#pragma once
#include <sys/epoll.h>

#include <thread>
#include <unordered_map>
#include <vector>

#include "PacketProtocol.hpp"

inline constexpr int LINE_SIZE = 100;
inline constexpr int LISTEN_QUEUE_SIZE = 20;
inline constexpr int EVENTS_MAX = 20;

class Server
{
 public:
  Server();
  ~Server();
  void init(uint16_t port);
  void run(int threadCount);

 private:
  struct Connection {
    struct MessageHandler {
      Connection& connection;
      void operator()(std::vector<uint8_t>& data) const;
    };

    Connection(int fd);

    int fd;
    uint32_t epollEvents = EPOLLIN | EPOLLET | EPOLLONESHOT;
    Net::PacketProtocol<MessageHandler> packetizer;
    std::vector<uint8_t> outBuffer;
  };

  int epfd;
  int listenfd;
  std::vector<std::thread> threads;
  pthread_rwlock_t connLatch;
  std::unordered_map<int, Connection> connections;
  void setNonBlocking(int socket);
  void closeConnection(int fd);
};
