#pragma once
#include <cstdint>
#include <vector>

namespace Net
{
typedef size_t packet_size_t;
typedef void (*MessageHandler)(std::vector<uint8_t> data);

class PacketProtocol
{
 public:
  static std::vector<uint8_t> wrapMessage(const uint8_t* msg, size_t msgLength);

  PacketProtocol(MessageHandler msgHandler);
  void receive(const uint8_t* data, size_t length);

 private:
  packet_size_t bytesReceived = 0;
  packet_size_t packetSize;
  bool readPacketSize = true;
  uint8_t sizeBuffer[sizeof(packet_size_t)];
  std::vector<uint8_t> dataBuffer;
  MessageHandler msgHandler;

  static std::vector<uint8_t> packetSizeToBytes(packet_size_t size);

  void readCompleted(size_t byteCount);
  packet_size_t packetSizeFromBuffer();
};
}  // namespace Net
