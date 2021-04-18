#include "TPCCParser.hpp"

#include <endian.h>

#include <algorithm>

namespace Net
{
void TPCCParser::parse(const uint8_t* data, const size_t length)
{
  size_t i = 0;

  while (i != length) {
    switch (funcID) {
      case TPCCFunctionID::notSet:
        // read function ID
        funcID = static_cast<TPCCFunctionID>(data[0]);
        // zero char arrays
        switch (funcID) {
          case TPCCFunctionID::orderStatusName:
            std::fill(&params.orderStatusName.c_last[0], &params.orderStatusName.c_last[16], 0);
            break;
          case TPCCFunctionID::paymentByName:
            std::fill(&params.paymentByName.c_last[0], &params.paymentByName.c_last[16], 0);
            break;
          default:
            break;
        }
        break;
      case TPCCFunctionID::newOrder:
        parseNewOrder(data[i]);
        break;
      case TPCCFunctionID::delivery:
        parseDelivery(data[i]);
        break;
      case TPCCFunctionID::stockLevel:
        parseStockLevel(data[i]);
        break;
      case TPCCFunctionID::orderStatusId:
        parseOrderStatusId(data[i]);
        break;
      case TPCCFunctionID::orderStatusName:
        parseOrderStatusName(data[i]);
        break;
      case TPCCFunctionID::paymentById:
        parsePaymentById(data[i]);
        break;
      case TPCCFunctionID::paymentByName:
        parsePaymentByName(data[i]);
        break;
    }
  }
  byteIndex++;
  i++;
}

inline void TPCCParser::setUpNewPaket()
{
  fieldIndex = 0;
  byteIndex = 0;
  funcID = TPCCFunctionID::notSet;
}

inline void TPCCParser::parseNewOrder(const uint8_t data)
{
  switch (fieldIndex) {
    case 0:
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

inline void TPCCParser::parseDelivery(const uint8_t data)
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

inline void TPCCParser::parseStockLevel(const uint8_t data)
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

inline void TPCCParser::parseOrderStatusId(const uint8_t data)
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

inline void TPCCParser::parseOrderStatusName(const uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.orderStatusName.w_id, data);
      break;
    case 1:
      parse32(params.orderStatusName.d_id, data);
      break;
    case 2:
      params.orderStatusName.c_last[byteIndex - 1] = data;
      if (byteIndex == params.orderStatusName.strSize) {
        runTPCCFunction();
        setUpNewPaket();
      }
      break;
  }
}

inline void TPCCParser::parsePaymentById(const uint8_t data)
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

inline void TPCCParser::parsePaymentByName(const uint8_t data)
{
  switch (fieldIndex) {
    case 0:
      parse32(params.paymentByName.w_id, data);
      break;
    case 1:
      parse32(params.paymentByName.d_id, data);
      break;
    case 2:
      parse32(params.paymentByName.c_w_id, data);
      break;
    case 3:
      parse32(params.paymentByName.c_d_id, data);
      break;
    case 4:
      params.paymentByName.c_last[byteIndex - 1] = data;
      if (byteIndex == params.paymentByName.strSize) {
        fieldIndex++;
        byteIndex = 0;
      }
      break;
    case 5:
      parse64(params.paymentByName.h_date, data);
      break;
    case 6:
      parse64(params.paymentByName.h_amount, data);
      break;
    case 7:
      parse64AndRun(params.paymentByName.datetime, data, [&]() {
        runTPCCFunction();
        setUpNewPaket();
      });
      break;
  }
}

inline void TPCCParser::parse32(uint32_t& dest, const uint8_t data)
{
  parse32AndRun(dest, data, [&]() {
    fieldIndex++;
    byteIndex = 0;
  });
}

inline void TPCCParser::parse64(uint64_t& dest, const uint8_t data)
{
  parse64AndRun(dest, data, [&]() {
    fieldIndex++;
    byteIndex = 0;
  });
}

inline void TPCCParser::parseVecElement(int32_t& dest, const uint8_t data)
{
  parse32AndRun(*reinterpret_cast<uint32_t*>(&dest), data, [&]() {
    vecIndex++;
    byteIndex = 0;
    if (vecIndex == params.newOrder.vecSize) {
      vecIndex == 0;
      fieldIndex++;
    }
  });
}
}  // namespace Net
