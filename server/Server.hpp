#pragma once
#include <cstdint>

inline constexpr int LINE_SIZE = 100;
inline constexpr int LISTEN_QUEUE_SIZE = 20;
inline constexpr int EVENTS_MAX = 20;

class Server
{
 public:
  void init(uint16_t port);
  void run(bool edge_triggered);

 private:
  bool edge_triggered;
  int epfd;
  int listenfd;
  void setNonBlocking(int socket);
  void handleET(int fd);
  void handleLT(int fd);
};
