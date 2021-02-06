#include "PacketProtocol.hpp"

namespace Net
{
std::vector<uint8_t> PacketProtocol::wrapMessage(const uint8_t* msg, size_t msgLength)
{
  auto packet = packetSizeToBytes(msgLength + sizeof(packet_size_t));
  packet.insert(packet.end(), msg, msg + msgLength);
  return packet;
}

PacketProtocol::PacketProtocol(MessageHandler msgHandler) : msgHandler(msgHandler) {}

std::vector<uint8_t> PacketProtocol::packetSizeToBytes(packet_size_t size)
{
  std::vector<uint8_t> bytes(sizeof(packet_size_t));
  for (int i = 0; i < sizeof(packet_size_t); i++)
    bytes[i] = (size >> (i * 8));
  return bytes;
}

void PacketProtocol::receive(const uint8_t* data, size_t length)
{
  int i = 0;
  while (i != length) {
    int bytesAvailable = length - i;
    if (dataBuffer.empty()) {
      // read into sizeBuffer
      int bytesToRead = sizeof(packet_size_t) - bytesReceived;
      int bytesCopied = std::min(bytesToRead, bytesAvailable);
      std::copy(data + i, data + i + bytesToRead, sizeBuffer + bytesReceived);
      i += bytesCopied;

      readCompleted(bytesCopied);
    } else {
      // read into dataBuffer
      int bytesToRead = packetSize - bytesReceived;
      int bytesCopied = std::min(bytesToRead, bytesAvailable);
      dataBuffer.insert(dataBuffer.end(), data + i, data + i + bytesToRead);
      i += bytesCopied;

      readCompleted(bytesCopied);
    }
  }
}

void PacketProtocol::readCompleted(size_t byteCount)
{
  bytesReceived += byteCount;

  if (readPacketSize) {
    if (bytesReceived == sizeof(packet_size_t)) {
      // received complete sizeBuffer
      packetSize = packetSizeFromBuffer();

      // TODO sanity check for max message size if one gets implemented
      
      // clear dataBuffer and start reading into it
      dataBuffer.clear();
      bytesReceived = 0;
      readPacketSize = false;
    }
  } else {
    if (bytesReceived == packetSize) {
      // received complete packet
      msgHandler(dataBuffer);

      // start reading into length buffer
      bytesReceived = 0;
      readPacketSize = true;
    }
  }
}

packet_size_t PacketProtocol::packetSizeFromBuffer()
{
  packet_size_t size = 0;
  for (int i = 0; i < sizeof(packet_size_t); i++)
    size += (sizeBuffer[i] << (i * 8));
  return size;
}
}  // namespace Net
