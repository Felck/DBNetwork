#include <iostream>
#include <string>

#include "Server.hpp"

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <-et | -lt> <port>" << std::endl;
    return 1;
  }

  bool edge_triggered;
  if (std::string(argv[1]) == "-et")
    edge_triggered = true;
  else if (std::string(argv[1]) == "-lt")
    edge_triggered = false;
  else {
    std::cout << "Usage: " << argv[0] << " <-et | -lt> <port>" << std::endl;
    return 1;
  }

  uint16_t port = std::stoi(argv[2]);

  Server server;
  server.init(port);
  server.run(edge_triggered);
  return 0;
}