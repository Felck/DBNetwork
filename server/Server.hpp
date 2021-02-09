#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "PacketProtocol.hpp"

inline constexpr int LINE_SIZE = 100;
inline constexpr int LISTEN_QUEUE_SIZE = 20;
inline constexpr int EVENTS_MAX = 20;

class Server
{
 public:
  void init(uint16_t port);
  void run();

 private:
  struct MessageHandler {
    int fd;
    std::unordered_map<int, std::vector<uint8_t>>& writeTasks;
    void operator()(std::vector<uint8_t>& data) const;
  };

 private:
  bool edge_triggered;
  int epfd;
  int listenfd;
  std::unordered_map<int, Net::PacketProtocol<MessageHandler>> connections;
  std::unordered_map<int, std::vector<uint8_t>> writeTasks;
  void setNonBlocking(int socket);
};
