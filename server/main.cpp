#include <iostream>
#include <string>

#include "Server.hpp"

int main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
    return 1;
  }

  uint16_t port = std::stoi(argv[1]);

  Server server;
  server.init(port);
  server.run();
  return 0;
}