#pragma once
#include <vector>

#include "ProtocolParser.hpp"
#include "endian.h"

namespace Net
{
enum class TPCCFunctionID : uint8_t {
  notSet = 0,
  newOrder = 1,
  delivery = 2,
  stockLevel = 3,
  orderStatusId = 4,
  orderStatusName = 5,
  paymentById = 6,
  paymentByName = 7
};

union TPCCFunctionParams {
  struct NewOrder {
    uint64_t timestamp;
    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_id;
    uint8_t vecSize;
  } newOrder;
  struct Delivery {
    uint64_t datetime;
    uint32_t w_id;
    uint32_t carrier_id;
  } delivery;
  struct StockLevel {
    uint32_t w_id;
    uint32_t d_id;
    uint32_t threshold;
  } stockLevel;
  struct OrderStatus {
    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_id;
  } orderStatusId;
  struct OrderStatusName {
    uint32_t w_id;
    uint32_t d_id;
    char c_last[16];
    uint8_t strSize;
  } orderStatusName;
  struct PaymentById {
    uint64_t h_date;
    uint64_t h_amount;
    uint64_t datetime;
    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_w_id;
    uint32_t c_d_id;
    uint32_t c_id;
  } paymentById;
  struct PaymentByName {
    uint64_t h_date;
    uint64_t h_amount;
    uint64_t datetime;
    uint32_t w_id;
    uint32_t d_id;
    uint32_t c_w_id;
    uint32_t c_d_id;
    char c_last[16];
    uint8_t strSize;
  } paymentByName;
};

struct TPCCVectorParams {
  std::vector<int32_t> lineNumbers;
  std::vector<int32_t> supwares;
  std::vector<int32_t> itemids;
  std::vector<int32_t> qtys;
};

class TPCCParser : ProtocolParser
{
 public:
  void parse(const uint8_t* data, const size_t length);

 private:
  size_t fieldIndex = 0;
  size_t vecIndex = 0;
  size_t byteIndex = 0;
  TPCCFunctionID funcID;
  TPCCFunctionParams params;
  TPCCVectorParams vParams;

  void runTPCCFunction() {}  // TODO tpcc function calls

  void setUpNewPaket();

  void parseNewOrder(const uint8_t data);
  void parseDelivery(const uint8_t data);
  void parseStockLevel(const uint8_t data);
  void parseOrderStatusId(const uint8_t data);
  void parseOrderStatusName(const uint8_t data);
  void parsePaymentById(const uint8_t data);
  void parsePaymentByName(const uint8_t data);

  void parse32(uint32_t& dest, const uint8_t data);
  void parse64(uint64_t& dest, const uint8_t data);
  void parseVecElement(int32_t& dest, const uint8_t data);

  template <typename Func>
  void parse32AndRun(uint32_t& dest, const uint8_t data, Func callback)
  {
    switch (byteIndex) {
      case 1:
        dest = static_cast<uint32_t>(data) << 24;
        break;
      case 2:
        dest |= static_cast<uint32_t>(data) << 16;
        break;
      case 3:
        dest |= static_cast<uint32_t>(data) << 8;
        break;
      case 4:
        dest |= static_cast<uint32_t>(data);
        dest = be32toh(dest);
        callback();
        break;
    }
  }

  template <typename Func>
  void parse64AndRun(uint64_t& dest, const uint8_t data, Func callBack)
  {
    switch (byteIndex) {
      case 1:
        dest = static_cast<uint64_t>(data) << 56;
        break;
      case 2:
        dest |= static_cast<uint64_t>(data) << 48;
        break;
      case 3:
        dest |= static_cast<uint64_t>(data) << 40;
        break;
      case 4:
        dest |= static_cast<uint64_t>(data) << 32;
        break;
      case 5:
        dest |= static_cast<uint64_t>(data) << 24;
        break;
      case 6:
        dest |= static_cast<uint64_t>(data) << 16;
        break;
      case 7:
        dest |= static_cast<uint64_t>(data) << 8;
        break;
      case 8:
        dest |= static_cast<uint64_t>(data);
        dest = be64toh(dest);
        callBack();
        break;
    }
  }
};
}  // namespace Net
