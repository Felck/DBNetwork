#pragma once
#include <cstddef>
#include <cstdint>

namespace Net
{
class ProtocolParser
{
 public:
  virtual ~ProtocolParser(){};
  virtual void parse(const uint8_t* data, const size_t length) = 0;
};
}  // namespace Net
