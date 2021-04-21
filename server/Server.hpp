#pragma once
#include <sys/epoll.h>

#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>

#include "PacketProtocol.hpp"

inline constexpr int LISTEN_QUEUE_SIZE = 20;

class Server
{
 public:
  Server();
  ~Server();
  void init(uint16_t port, size_t bufferSize);
  void run(int threadCount);
  void runThread();

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
  size_t bufferSize;
  std::vector<std::thread> threads;
  pthread_rwlock_t connLatch;
  std::unordered_map<int, Connection> connections;

  static std::atomic<uint64_t> eventCounter;

  void setNonBlocking(int socket);
  void closeConnection(int fd);
};
