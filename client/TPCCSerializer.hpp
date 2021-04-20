#include <vector>

#include "types.hpp"

namespace TPCC
{
std::vector<uint8_t> serializeNewOrder(Integer w_id,
                  Integer d_id,
                  Integer c_id,
                  const std::vector<Integer>& lineNumbers,
                  const std::vector<Integer>& supwares,
                  const std::vector<Integer>& itemids,
                  const std::vector<Integer>& qtys,
                  Timestamp timestamp);
std::vector<uint8_t> serializeDelivery(Integer w_id, Integer carrier_id, Timestamp datetime);
std::vector<uint8_t> serializeStockLevel(Integer w_id, Integer d_id, Integer threshold);
std::vector<uint8_t> serializeOrderStatusId(Integer w_id, Integer d_id, Integer c_id);
std::vector<uint8_t> serializeOrderStatusName(Integer w_id, Integer d_id, Varchar<16> c_last);
std::vector<uint8_t> serializePaymentById(Integer w_id,
                     Integer d_id,
                     Integer c_w_id,
                     Integer c_d_id,
                     Integer c_id,
                     Timestamp h_date,
                     Numeric h_amount,
                     Timestamp datetime);
std::vector<uint8_t> serializePaymentByName(Integer w_id,
                       Integer d_id,
                       Integer c_w_id,
                       Integer c_d_id,
                       Varchar<16> c_last,
                       Timestamp h_date,
                       Numeric h_amount,
                       Timestamp datetime);
}  // namespace TPCC
