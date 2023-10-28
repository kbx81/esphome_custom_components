#pragma once

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_color_values.h"
#include "esphome/components/light/light_transformer.h"
#include "esphome/components/light/color_mode.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"

#include <cinttypes>

namespace esphome {
namespace dop_led_plus_h_bridge {

/// RGB color channel orderings, used when instantiating controllers to determine
/// what order the controller should send data out in. The default ordering
/// is RGB.
/// Within this enum, the red channel is 0, the green channel is 1, and the
/// blue chanel is 2.
enum EOrder {
  RGB = 0012,  ///< Red,   Green, Blue  (0012)
  RBG = 0021,  ///< Red,   Blue,  Green (0021)
  GRB = 0102,  ///< Green, Red,   Blue  (0102)
  GBR = 0120,  ///< Green, Blue,  Red   (0120)
  BRG = 0201,  ///< Blue,  Red,   Green (0201)
  BGR = 0210   ///< Blue,  Green, Red   (0210)
};

class DoPLEDOutput : public light::AddressableLight {
 public:
  DoPLEDOutput(remote_transmitter::RemoteTransmitterComponent *transmitter, output::BinaryOutput *output_p2,
               output::FloatOutput *output_n1_pwm, output::BinaryOutput *output_n2, output::BinaryOutput *output_2v5)
      : transmit_call_(new remote_transmitter::RemoteTransmitterComponent::TransmitCall(transmitter->transmit())),
        transmitter_(transmitter),
        output_p2_(output_p2),
        output_n1_pwm_(output_n1_pwm),
        output_n2_(output_n2),
        output_2v5_(output_2v5) {
    this->transmitter_->set_carrier_duty_percent(100);
  };

  inline int32_t size() const override { return this->num_leds_; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  /// set the number of header bits to send
  void set_num_header_bits(size_t num_header_bits) { this->num_header_bits_ = num_header_bits; }

  /// set the number of LED "channels" for this controller
  void set_num_leds(size_t num_leds) {
    this->num_leds_ = num_leds;
    this->leds_ = new Color[num_leds];  // NOLINT

    for (int i = 0; i < this->num_leds_; i++)
      this->leds_[i] = Color::BLACK;
  }

  /// set the RGB order for LEDs on this controller
  void set_rgb_order(EOrder order) { this->order_ = order; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
    return traits;
  }

  void setup() override;
  void dump_config() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  std::unique_ptr<light::LightTransformer> create_default_transition() override;

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

 protected:
  friend class DoPLEDLightTransformer;

  light::ESPColorView get_view_internal(int32_t index) const override {
    return {&this->leds_[index].r,      &this->leds_[index].g, &this->leds_[index].b, nullptr,
            &this->effect_data_[index], &this->correction_};
  }

  Color *leds_{nullptr};
  EOrder order_{RGB};
  uint8_t *effect_data_{nullptr};
  uint8_t num_header_bits_{0};
  uint8_t num_leds_{0};
  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
  light::ColorMode prev_color_mode_{};
  remote_transmitter::RemoteTransmitterComponent::TransmitCall *transmit_call_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  output::BinaryOutput *output_p2_{nullptr};
  output::FloatOutput *output_n1_pwm_{nullptr};
  output::BinaryOutput *output_n2_{nullptr};
  output::BinaryOutput *output_2v5_{nullptr};
};

class DoPLEDLightTransformer : public light::LightTransitionTransformer {
 public:
  DoPLEDLightTransformer(DoPLEDOutput &light) : light_(light) {}

  void start() override;
  optional<light::LightColorValues> apply() override;

 protected:
  DoPLEDOutput &light_;
  Color target_color_{};
  float last_transition_progress_{0.0f};
  float accumulated_alpha_{0.0f};
};

}  // namespace dop_led_plus_h_bridge

namespace remote_base {

struct DoPLEDData {
  uint8_t num_header_bits;
  uint8_t num_leds;
  dop_led_plus_h_bridge::EOrder order;
  Color *leds;
};

class DoPLEDProtocol : public RemoteProtocol<DoPLEDData> {
 public:
  void encode(RemoteTransmitData *dst, const DoPLEDData &data) override;
  optional<DoPLEDData> decode(RemoteReceiveData src) override { return nullopt; }
  void dump(const DoPLEDData &data) override {}
};

}  // namespace remote_base
}  // namespace esphome