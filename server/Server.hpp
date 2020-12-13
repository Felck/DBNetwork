#pragma once

inline constexpr int LINE_SIZE = 100;
inline constexpr int LISTEN_QUEUE_SIZE = 20;
inline constexpr int EVENTS_MAX = 20;
inline constexpr int WAIT_TIMEOUT = 500;  // in ms, -1 means no timeout

class Server
{
 public:
  void run();

 private:
  int epfd;
  int listenfd;
  void init();
  void setNonBlocking(int socket);
  void readFromSocket(int socket);
};
