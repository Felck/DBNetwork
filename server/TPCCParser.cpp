#include "TPCCParser.hpp"

namespace TPCC
{
std::atomic<uint64_t> eventCounter = 0;

void Parser::parse(const uint8_t* data, size_t length)
{
  size_t i = 0;

  while (i != length) {
    switch (funcID) {
      case FunctionID::notSet:
        // new paket: read function ID
        funcID = static_cast<FunctionID>(data[i]);
        byteIndex = 0;
        break;
      case FunctionID::newOrder:
        parseNewOrder(data[i]);
        break;
      case FunctionID::delivery:
        parseDelivery(data[i]);
        break;
      case FunctionID::stockLevel:
        parseStockLevel(data[i]);
        break;
      case FunctionID::orderStatusId:
        parseOrderStatusId(data[i]);
        break;
      case FunctionID::orderStatusName:
        parseOrderStatusName(data[i]);
        break;
      case FunctionID::paymentById:
        parsePaymentById(data[i]);
        break;
      case FunctionID::paymentByName:
        parsePaymentByName(data[i]);
        break;
    }
    byteIndex++;
    i++;
  }
}

inline void Parser::setUpNewPaket()
{
  fieldIndex = 0;
  byteIndex = 0;
  funcID = FunctionID::notSet;
}

inline void Parser::parseNewOrder(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      // read vector lengths and resize
      params.newOrder.vecSize = data;
      vParams.lineNumbers.resize(data);
      vParams.supwares.resize(data);
      vParams.itemids.resize(data);
      vParams.qtys.resize(data);
      vecIndex = 0;
      fieldIndex++;
      byteIndex = 0;
      break;
    case 1:
      parse32(params.newOrder.w_id, data);
      break;
    case 2:
      parse32(params.newOrder.d_id, data);
      break;
    case 3:
      parse32(params.newOrder.c_id, data);
      break;
    case 4:
      parseVecElement(vParams.lineNumbers.at(vecIndex), data);
      break;
    case 5:
      parseVecElement(vParams.supwares.at(vecIndex), data);
      break;
    case 6:
      parseVecElement(vParams.itemids.at(vecIndex), data);
      break;
    case 7:
      parseVecElement(vParams.qtys.at(vecIndex), data);
      break;
    case 8:
      parse64AndRun(params.newOrder.timestamp, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parseDelivery(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.delivery.w_id, data);
      break;
    case 1:
      parse32(params.delivery.carrier_id, data);
      break;
    case 2:
      parse64AndRun(params.delivery.datetime, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parseStockLevel(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.stockLevel.w_id, data);
      break;
    case 1:
      parse32(params.stockLevel.d_id, data);
      break;
    case 2:
      parse32AndRun(params.stockLevel.threshold, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parseOrderStatusId(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.orderStatusId.w_id, data);
      break;
    case 1:
      parse32(params.orderStatusId.d_id, data);
      break;
    case 2:
      parse32AndRun(params.orderStatusId.c_id, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parseOrderStatusName(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      // read strLength and zero char array
      params.orderStatusName.strLength = data;
      std::fill(&params.orderStatusName.c_last[0], &params.orderStatusName.c_last[16], 0);
      fieldIndex++;
      byteIndex = 0;
      break;
    case 1:
      parse32(params.orderStatusName.w_id, data);
      break;
    case 2:
      parse32(params.orderStatusName.d_id, data);
      break;
    case 3:
      // read char array
      params.orderStatusName.c_last[byteIndex - 1] = data;
      if (byteIndex == params.orderStatusName.strLength) {
        runTPCCFunction();
        setUpNewPaket();
      }
      break;
  }
}

inline void Parser::parsePaymentById(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.paymentById.w_id, data);
      break;
    case 1:
      parse32(params.paymentById.d_id, data);
      break;
    case 2:
      parse32(params.paymentById.c_w_id, data);
      break;
    case 3:
      parse32(params.paymentById.c_d_id, data);
      break;
    case 4:
      parse32(params.paymentById.c_id, data);
      break;
    case 5:
      parse64(params.paymentById.h_date, data);
      break;
    case 6:
      parse64(params.paymentById.h_amount, data);
      break;
    case 7:
      parse64AndRun(params.paymentById.datetime, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parsePaymentByName(uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      // read strLength and zero char array
      params.paymentByName.strLength = data;
      std::fill(&params.paymentByName.c_last[0], &params.paymentByName.c_last[16], 0);
      fieldIndex++;
      byteIndex = 0;
      break;
    case 1:
      parse32(params.paymentByName.w_id, data);
      break;
    case 2:
      parse32(params.paymentByName.d_id, data);
      break;
    case 3:
      parse32(params.paymentByName.c_w_id, data);
      break;
    case 4:
      parse32(params.paymentByName.c_d_id, data);
      break;
    case 5:
      // read char array
      params.paymentByName.c_last[byteIndex - 1] = data;
      if (byteIndex == params.paymentByName.strLength) {
        fieldIndex++;
        byteIndex = 0;
      }
      break;
    case 6:
      parse64(params.paymentByName.h_date, data);
      break;
    case 7:
      parse64(params.paymentByName.h_amount, data);
      break;
    case 8:
      parse64AndRun(params.paymentByName.datetime, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void Parser::parse32(uint32_t& dest, uint8_t data)
{
  parse32AndRun(dest, data, [&]() {
    fieldIndex++;
    byteIndex = 0;
  });
}

inline void Parser::parse64(uint64_t& dest, uint8_t data)
{
  parse64AndRun(dest, data, [&]() {
    fieldIndex++;
    byteIndex = 0;
  });
}

inline void Parser::parseVecElement(int32_t& dest, uint8_t data)
{
  parse32AndRun(*reinterpret_cast<uint32_t*>(&dest), data, [&]() {
    vecIndex++;
    byteIndex = 0;
    if (vecIndex == params.newOrder.vecSize) {
      vecIndex = 0;
      fieldIndex++;
    }
  });
}
}  // namespace TPCC
