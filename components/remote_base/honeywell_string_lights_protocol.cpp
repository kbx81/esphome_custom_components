#include "honeywell_string_lights_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.honeywell_string_lights";

static const uint8_t NBITS = 16;
static const uint32_t HEADER_HIGH_US = 2000;
static const uint32_t HEADER_LOW_US = 550;
static const uint32_t BIT_ONE_LOW_US = 1000;
static const uint32_t BIT_ZERO_LOW_US = 450;
static const uint32_t BIT_HIGH_US = 550;

void HSLProtocol::encode(RemoteTransmitData *dst, const HSLData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + NBITS * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint32_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (data.data & mask) {
      dst->item(BIT_ONE_LOW_US, BIT_HIGH_US);
    } else {
      dst->item(BIT_ZERO_LOW_US, BIT_HIGH_US);
    }
  }
}

optional<HSLData> HSLProtocol::decode(RemoteReceiveData src) {
  HSLData out{.data = 0};
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint8_t i = NBITS; i > 0; i--) {
    out.data <<= 1UL;
    if (src.expect_mark(BIT_ONE_LOW_US)) {
      out.data |= 1UL;
    } else if (src.expect_mark(BIT_ZERO_LOW_US)) {
      out.data |= 0UL;
    } else {
      return {};
    }

    if (i > 1) {
      if (!src.expect_space(BIT_HIGH_US)) {
        return {};
      }
    }
  }
  return out;
}

void HSLProtocol::dump(const HSLData &data) { ESP_LOGD(TAG, "Received HSL: data=0x%04" PRIX32, data.data); }

}  // namespace remote_base
}  // namespace esphome
