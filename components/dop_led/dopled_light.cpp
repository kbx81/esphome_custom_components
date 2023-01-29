#ifdef USE_ARDUINO

#include "dopled_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dop_led {

static const char *const TAG = "dop_led";

void DoPLEDOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DoPLED light...");
  this->effect_data_ = new uint8_t[this->num_leds_];  // NOLINT
}

void DoPLEDOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "DoPLED light:");
  ESP_LOGCONFIG(TAG, "  Num LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Num header bits: %u", this->num_header_bits_);
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %u", *this->max_refresh_rate_);
}

void DoPLEDOutput::write_state(light::LightState *state) {
  // protect from refreshing too often
  uint32_t now = micros();
  if (this->max_refresh_rate_.has_value()) {
    if (this->max_refresh_rate_.value() != 0 && (now - this->last_refresh_) < this->max_refresh_rate_.value()) {
      // try again next loop iteration, so that this change won't get lost
      this->schedule_show();
      return;
    }
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  if (this->transmitter_ == nullptr) {
    ESP_LOGD(TAG, "RMT not set, cannot write RGB values to bus!");
  } else {
    ESP_LOGVV(TAG, "Writing RGB values to bus...");
    this->transmit_call_.get_data()->reset();
    remote_base::DoPLEDData xmit_data{this->num_header_bits_, this->num_leds_, this->order_, this->leds_};
    remote_base::DoPLEDProtocol().encode(this->transmit_call_.get_data(), xmit_data);
    this->transmit_call_.set_send_times(0);
    this->transmit_call_.perform();
  }
}

}  // namespace dop_led

namespace remote_base {

// these values are absolute minimums that seem to work consistently and reliably
static const uint8_t BIT_ONE_HIGH_US = 135;
static const uint8_t BIT_ZERO_HIGH_US = 80;
static const uint8_t BIT_LOW_US = 80;
static const uint16_t FOOTER_MARK_US = 250;

void DoPLEDProtocol::encode(RemoteTransmitData *dst, const DoPLEDData &data) {
  // (8 bits for each of R, G, B + header bits + footer bit) * 2 (because high/low)
  dst->reserve(((8 * 3) + data.num_header_bits + 1) * 2 * data.num_leds);
  dst->set_carrier_frequency(0);

  for (size_t led = 0; led < data.num_leds; led++) {
    for (uint32_t bit = 0; bit < data.num_header_bits; bit++) {
      if (led & (1 << bit)) {
        dst->item(BIT_ONE_HIGH_US, BIT_LOW_US);
      } else {
        dst->item(BIT_ZERO_HIGH_US, BIT_LOW_US);
      }
    }

    for (size_t col_i = 2; col_i >= 0 && col_i < 3; col_i--) {
      size_t col_attr_i = (data.order >> (col_i * 3)) & 7;

      for (uint8_t mask = 1; mask; mask <<= 1) {
        if (data.leds[led].raw[col_attr_i] & mask) {
          dst->item(BIT_ONE_HIGH_US, BIT_LOW_US);
        } else {
          dst->item(BIT_ZERO_HIGH_US, BIT_LOW_US);
        }
      }
    }
    dst->item(FOOTER_MARK_US, FOOTER_MARK_US);
  }
}

}  // namespace remote_base
}  // namespace esphome

#endif  // USE_ARDUINO
