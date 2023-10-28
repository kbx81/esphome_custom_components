#pragma once

#include <cinttypes>

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct HSLData {
  uint32_t data;

  bool operator==(const HSLData &rhs) const { return data == rhs.data; }
};

class HSLProtocol : public RemoteProtocol<HSLData> {
 public:
  void encode(RemoteTransmitData *dst, const HSLData &data) override;
  optional<HSLData> decode(RemoteReceiveData src) override;
  void dump(const HSLData &data) override;
};

DECLARE_REMOTE_PROTOCOL(HSL)

template<typename... Ts> class HSLAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    HSLData data{};
    data.data = this->data_.value(x...);
    HSLProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
