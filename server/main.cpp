#include <iostream>
#include <string>

#include "Server.hpp"

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <port> <number of threads>" << std::endl;
    return 1;
  }

  uint16_t port = std::stoi(argv[1]);
  int threads = std::stoi(argv[2]);

  Server server;
  server.init(port);
  server.run(threads);
  return 0;
}