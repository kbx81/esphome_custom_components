#include "dop_led_plus_h_bridge.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dop_led_plus_h_bridge {

static const char *const TAG = "dop_led_plus_h_bridge";

void DoPLEDOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DoPLED light...");
  this->effect_data_ = new uint8_t[this->num_leds_];  // NOLINT
}

void DoPLEDOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "DoPLED light:");
  ESP_LOGCONFIG(TAG, "  Num LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Num header bits: %u", this->num_header_bits_);
  if (this->max_refresh_rate_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Max refresh rate: %u", this->max_refresh_rate_.value());
  }
}

void DoPLEDOutput::write_state(light::LightState *state) {
  auto color_mode = state->current_values.get_color_mode();
  uint32_t now = micros();
  float brightness;
  state->current_values_as_brightness(&brightness);
  if (state->current_values.is_on()) {
    switch (color_mode) {
      case light::ColorMode::RGB:
        if (color_mode != this->prev_color_mode_) {
          this->prev_color_mode_ = color_mode;                // save new mode
          this->output_n1_pwm_->set_level(0);                 // turn off conflicting drivers
          this->output_p2_->turn_off();                       // turn off conflicting drivers
          delay(2);                                           // let currents settle
          this->output_n2_->turn_on();                        // turn on required drivers
          this->transmitter_->set_rmt_force_inverted(false);  // "enable" the RMT
          this->output_2v5_->turn_on();                       // turn on required drivers
        }

        // protect from refreshing too often
        if (this->max_refresh_rate_.has_value()) {
          if (this->max_refresh_rate_.value() != 0 && (now - this->last_refresh_) < this->max_refresh_rate_.value()) {
            // try again next loop iteration, so that this change won't get lost
            this->schedule_show();
            return;
          }
        }

        if (this->transmitter_ == nullptr) {
          ESP_LOGD(TAG, "RMT not set, cannot write RGB values to bus!");
        } else {
          ESP_LOGVV(TAG, "Writing RGB values to bus...");
          this->transmit_call_->get_data()->reset();
          remote_base::DoPLEDData xmit_data{this->num_header_bits_, this->num_leds_, this->order_, this->leds_};
          remote_base::DoPLEDProtocol().encode(this->transmit_call_->get_data(), xmit_data);
          this->transmit_call_->set_send_times(0);
          this->transmit_call_->perform();
        }
        break;

      case light::ColorMode::WHITE:
        if (color_mode != this->prev_color_mode_) {
          this->prev_color_mode_ = color_mode;               // save new mode
          this->output_2v5_->turn_off();                     // turn off conflicting drivers
          this->output_n2_->turn_off();                      // turn off conflicting drivers
          this->transmitter_->set_rmt_force_inverted(true);  // "disable" the RMT
          delay(2);                                          // let currents settle
          this->output_n1_pwm_->set_level(brightness);       // set brightness
          this->output_p2_->turn_on();                       // turn on required drivers
        } else {
          this->output_n1_pwm_->set_level(brightness);  // set brightness
        }
        break;

      default:
        break;
    }
    this->last_refresh_ = now;
    this->mark_shown_();
  } else {  // turn everything off
    this->prev_color_mode_ = light::ColorMode::UNKNOWN;
    this->output_2v5_->turn_off();
    this->output_n2_->turn_off();
    this->transmitter_->set_rmt_force_inverted(true);
    this->output_n1_pwm_->set_level(0);
    this->output_p2_->turn_off();
  }
}

std::unique_ptr<light::LightTransformer> DoPLEDOutput::create_default_transition() {
  return make_unique<DoPLEDLightTransformer>(*this);
}

void DoPLEDLightTransformer::start() {
  // don't try to transition over running effects.
  if (this->light_.is_effect_active())
    return;

  // When turning light on from off state, use target state and only increase brightness from zero.
  if (!this->start_values_.is_on() && this->target_values_.is_on()) {
    this->start_values_ = light::LightColorValues(this->target_values_);
    this->start_values_.set_brightness(0.0f);
  }

  // When turning light off from on state, use source state and only decrease brightness to zero. Use a second
  // variable for transition end state, as overwriting target_values breaks LightState logic.
  if (this->start_values_.is_on() && !this->target_values_.is_on()) {
    this->end_values_ = light::LightColorValues(this->start_values_);
    this->end_values_.set_brightness(0.0f);
  } else {
    this->end_values_ = light::LightColorValues(this->target_values_);
  }

  // When changing color mode, go through off state, as color modes are orthogonal and there can't be two active.
  if (this->start_values_.get_color_mode() != this->target_values_.get_color_mode()) {
    this->changing_color_mode_ = true;
    this->intermediate_values_ = this->start_values_;
    this->intermediate_values_.set_state(false);
  }

  auto end_values = this->target_values_;
  this->target_color_ = color_from_light_color_values(end_values);

  // our transition will handle brightness, disable brightness in correction.
  this->light_.correction_.set_local_brightness(255);
  this->target_color_ *= light::to_uint8_scale(end_values.get_brightness() * end_values.get_state());
}

optional<light::LightColorValues> DoPLEDLightTransformer::apply() {
  float p = this->get_progress_();
  float smoothed_progress = light::LightTransitionTransformer::smoothed_progress(p);

  // When running an output-buffer modifying effect, don't try to transition individual LEDs, but instead just fade the
  // LightColorValues. write_state() then picks up the change in brightness, and the color change is picked up by the
  // effects which respect it.
  if (this->light_.is_effect_active())
    return light::LightColorValues::lerp(this->get_start_values(), this->get_target_values(), smoothed_progress);

  // Use a specialized transition for addressable lights: instead of using a unified transition for
  // all LEDs, we use the current state of each LED as the start.

  // We can't use a direct lerp smoothing here though - that would require creating a copy of the original
  // state of each LED at the start of the transition.
  // Instead, we "fake" the look of the LERP by using an exponential average over time and using
  // dynamically-calculated alpha values to match the look.

  float denom = (1.0f - smoothed_progress);
  float alpha = denom == 0.0f ? 1.0f : (smoothed_progress - this->last_transition_progress_) / denom;

  // We need to use a low-resolution alpha here which makes the transition set in only after ~half of the length
  // We solve this by accumulating the fractional part of the alpha over time.
  float alpha255 = alpha * 255.0f;
  float alpha255int = floorf(alpha255);
  float alpha255remainder = alpha255 - alpha255int;

  this->accumulated_alpha_ += alpha255remainder;
  float alpha_add = floorf(this->accumulated_alpha_);
  this->accumulated_alpha_ -= alpha_add;

  alpha255 += alpha_add;
  alpha255 = clamp(alpha255, 0.0f, 255.0f);
  auto alpha8 = static_cast<uint8_t>(alpha255);

  if (alpha8 != 0) {
    uint8_t inv_alpha8 = 255 - alpha8;
    Color add = this->target_color_ * alpha8;

    for (auto led : this->light_)
      led.set(add + led.get() * inv_alpha8);
  }

  this->last_transition_progress_ = smoothed_progress;
  this->light_.schedule_show();

  // Halfway through, when intermediate state (off) is reached, flip it to the target, but remain off.
  if (this->changing_color_mode_ && p > 0.5f &&
      this->intermediate_values_.get_color_mode() != this->target_values_.get_color_mode()) {
    this->intermediate_values_ = this->target_values_;
    this->intermediate_values_.set_state(false);
  }

  light::LightColorValues &start =
      this->changing_color_mode_ && p > 0.5f ? this->intermediate_values_ : this->start_values_;
  light::LightColorValues &end =
      this->changing_color_mode_ && p < 0.5f ? this->intermediate_values_ : this->end_values_;
  if (this->changing_color_mode_)
    p = p < 0.5f ? p * 2 : (p - 0.5) * 2;

  float v = light::LightTransitionTransformer::smoothed_progress(p);
  return light::LightColorValues::lerp(start, end, v);
}

}  // namespace dop_led_plus_h_bridge

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

    for (size_t col_i = 2; col_i < 3; col_i--) {
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