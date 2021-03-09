#include "PacketProtocol.hpp"

namespace Net
{
std::vector<uint8_t> wrapMessage(const uint8_t* message, const size_t length)
{
  std::vector<uint8_t> packet;
  // insert length prefix
  for (int i = 0; i < sizeof(packet_size_t); i++)
    packet.push_back(length >> (i * 8));
  // insert message
  packet.insert(packet.end(), message, message + length);
  return packet;
}
}  // namespace Net
