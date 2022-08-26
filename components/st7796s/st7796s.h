#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace st7796s {

class ST7796S : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  ST7796S(int width, int height, int colstart, int rowstart, bool eightbitcolor, bool usebgr,
         bool invert_colors);
  void dump_config() override;
  void setup() override;

  void display();

  void update() override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_backlight_pin(GPIOPin *backlight_pin) { this->backlight_pin_ = backlight_pin; }

  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_dc_pin(GPIOPin *value) { dc_pin_ = value; }
  size_t get_buffer_length();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes);
  void senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes);

  void writecommand_(uint8_t value);
  void writedata_(uint8_t value);

  void write_display_data_();

  void init_reset_();
  void backlight_(bool onoff);
  void display_init_(const uint8_t *addr);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void spi_master_write_addr_(uint16_t addr1, uint16_t addr2);
  void spi_master_write_color_(uint16_t color, uint16_t size);

  int get_width_internal() override;
  int get_height_internal() override;

  void setRotation(uint_fast8_t r);

  uint8_t colstart_ = 0, rowstart_ = 0;
  bool eightbitcolor_ = false;
  bool usebgr_ = false;
  bool invert_colors_ = false;
  int16_t width_ = 80, height_ = 80;  // Watch heap size

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};
};

}  // namespace st7735
}  // namespace esphome
