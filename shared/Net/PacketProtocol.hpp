#pragma once
#include <cstdint>
#include <vector>

namespace Net
{
typedef size_t packet_size_t;

// convert msg to byte vector with length prefix
std::vector<uint8_t> wrapMessage(const uint8_t* msg, size_t msgLength);

// class to decode length prefixed byte messages
template <typename Func>
class PacketProtocol
{
 public:
  // construct with msgHandler that gets called for every completely received message
  PacketProtocol(Func msgHandler) : msgHandler(msgHandler) {}

  // read (partial) length prefixed data in and call msgHandler whenever a complete packet was received
  void receive(const uint8_t* data, size_t length)
  {
    int i = 0;
    while (i != length) {
      int bytesAvailable = length - i;
      if (readPacketSize) {
        // read into sizeBuffer
        int bytesToRead = sizeof(packet_size_t) - bytesReceived;
        int bytesCopied = std::min(bytesToRead, bytesAvailable);
        std::copy(data + i, data + i + bytesToRead, sizeBuffer + bytesReceived);
        i += bytesCopied;
        bytesReceived += bytesCopied;

        if (bytesReceived == sizeof(packet_size_t)) {
          // received complete packetSize
          packetSize = 0;
          for (int i = 0; i < sizeof(packet_size_t); i++)
            packetSize += (sizeBuffer[i] << (i * 8));

          // clear dataBuffer and start reading into it
          dataBuffer.clear();
          bytesReceived = 0;
          readPacketSize = false;
        }
      } else {
        // read into dataBuffer
        int bytesToRead = packetSize - bytesReceived;
        int bytesCopied = std::min(bytesToRead, bytesAvailable);
        dataBuffer.insert(dataBuffer.end(), data + i, data + i + bytesToRead);
        i += bytesCopied;
        bytesReceived += bytesCopied;

        if (bytesReceived == packetSize) {
          // received complete packet
          msgHandler(dataBuffer);

          // start reading into length buffer
          bytesReceived = 0;
          readPacketSize = true;
        }
      }
    }
  }

 private:
  packet_size_t bytesReceived = 0;
  packet_size_t packetSize;
  bool readPacketSize = true;
  uint8_t sizeBuffer[sizeof(packet_size_t)];
  std::vector<uint8_t> dataBuffer;
  const Func msgHandler;
};
}  // namespace Net
