#include <iostream>
#include <string>

#include "Server.hpp"

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " <port> <number of threads> <read line size>" << std::endl;
    return 1;
  }

  uint16_t port = std::stoi(argv[1]);
  int threads = std::stoi(argv[2]);
  size_t lineSize = std::stoi(argv[3]);

  Server server;
  server.init(port);
  server.run(threads, lineSize);
  return 0;
}