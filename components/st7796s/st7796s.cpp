#include "st7796s.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace st7796s {

static const uint8_t ST_CMD_DELAY = 0x80;  // special signifier for command lists

static const uint8_t ST77XX_NOP = 0x00;
static const uint8_t ST77XX_SWRESET = 0x01;
static const uint8_t ST77XX_RDDID = 0x04;
static const uint8_t ST77XX_RDDST = 0x09;

static const uint8_t ST77XX_SLPIN = 0x10;
static const uint8_t ST77XX_SLPOUT = 0x11;
static const uint8_t ST77XX_PTLON = 0x12;
static const uint8_t ST77XX_NORON = 0x13;

static const uint8_t ST77XX_INVOFF = 0x20;
static const uint8_t ST77XX_INVON = 0x21;
static const uint8_t ST77XX_DISPOFF = 0x28;
static const uint8_t ST77XX_DISPON = 0x29;
static const uint8_t ST77XX_CASET = 0x2A;
static const uint8_t ST77XX_RASET = 0x2B;
static const uint8_t ST77XX_RAMWR = 0x2C;
static const uint8_t ST77XX_RAMRD = 0x2E;

static const uint8_t ST77XX_PTLAR = 0x30;
static const uint8_t ST77XX_TEOFF = 0x34;
static const uint8_t ST77XX_TEON = 0x35;
static const uint8_t ST77XX_MADCTL = 0x36;
static const uint8_t ST77XX_COLMOD = 0x3A;

static const uint8_t ST77XX_MADCTL_MY = 0x80;
static const uint8_t ST77XX_MADCTL_MX = 0x40;
static const uint8_t ST77XX_MADCTL_MV = 0x20;
static const uint8_t ST77XX_MADCTL_ML = 0x10;
static const uint8_t ST77XX_MADCTL_RGB = 0x00;

static const uint8_t ST77XX_RDID1 = 0xDA;
static const uint8_t ST77XX_RDID2 = 0xDB;
static const uint8_t ST77XX_RDID3 = 0xDC;
static const uint8_t ST77XX_RDID4 = 0xDD;

static const uint8_t ST7796S_FRMCTR1 = 0xB1;
static const uint8_t ST7796S_FRMCTR2 = 0xB2;
static const uint8_t ST7796S_FRMCTR3 = 0xB3;
static const uint8_t ST7796S_INVCTR  = 0xB4;
static const uint8_t ST7796S_DFUNCTR = 0xB6;
static const uint8_t ST7796S_ETMOD   = 0xB7;
static const uint8_t ST7796S_PWCTR1  = 0xC0;
static const uint8_t ST7796S_PWCTR2  = 0xC1;
static const uint8_t ST7796S_PWCTR3  = 0xC2;
static const uint8_t ST7796S_PWCTR4  = 0xC3;
static const uint8_t ST7796S_PWCTR5  = 0xC4;
static const uint8_t ST7796S_VMCTR   = 0xC5;
static const uint8_t ST7796S_GMCTRP1 = 0xE0; // Positive Gamma Correction
static const uint8_t ST7796S_GMCTRN1 = 0xE1; // Negative Gamma Correction
static const uint8_t ST7796S_DOCA    = 0xE8; // Display Output Ctrl Adjust 
static const uint8_t ST7796S_CSCON   = 0xF0; // Command Set Control

// clang-format off
static const uint8_t PROGMEM

  BCMD[] = {
    16,
    ST77XX_SWRESET, ST_CMD_DELAY, 120, 
    ST77XX_SLPOUT,  ST_CMD_DELAY, 120, 
    ST7796S_CSCON,   1, 0xC3,  // Enable extension command 2 partI
    ST7796S_CSCON,   1, 0x96,  // Enable extension command 2 partII
    ST7796S_INVCTR,  1, 0x01,  //1-dot inversion
    ST7796S_DFUNCTR, 3, 0x80,  //Display Function Control //Bypass
                      0x02,  //Source Output Scan from S1 to S960, Gate Output scan from G1 to G480, scan cycle=2
                      0x3B,  //LCD Drive Line=8*(59+1)
    ST7796S_DOCA,    8, 0x40,
                      0x8A,
                      0x00,
                      0x00,
                      0x29,  //Source eqaulizing period time= 22.5 us
                      0x19,  //Timing for "Gate start"=25 (Tclk)
                      0xA5,  //Timing for "Gate End"=37 (Tclk), Gate driver EQ function ON
                      0x33,
    ST7796S_PWCTR2,  1, 0x06,  //Power control2   //VAP(GVDD)=3.85+( vcom+vcom offset), VAN(GVCL)=-3.85+( vcom+vcom offset)
    ST7796S_PWCTR3,  1, 0xA7,  //Power control 3  //Source driving current level=low, Gamma driving current level=High
    ST7796S_VMCTR,   1 + ST_CMD_DELAY, 0x18, 120, //VCOM Control    //VCOM=0.9
    ST7796S_GMCTRP1,14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
                      0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B,
    ST7796S_GMCTRN1,14+ST_CMD_DELAY, 
                      0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
                      0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B,
                      120,
    ST7796S_CSCON,   1, 0x3C, //Command Set control // Disable extension command 2 partI
    ST7796S_CSCON,   1, 0x69, //Command Set control // Disable extension command 2 partII
    ST77XX_SLPOUT,  ST_CMD_DELAY, 120,    // Exit sleep mode
    ST77XX_DISPON,  ST_CMD_DELAY,  20,    // Set display on
  };                        //     255 = max (500 ms) delay

// clang-format on
static const char *const TAG = "st7796s";

enum mad_t
{ MAD_MY  = 0x80
, MAD_MX  = 0x40
, MAD_MV  = 0x20
, MAD_ML  = 0x10
, MAD_BGR = 0x08
, MAD_MH  = 0x04
, MAD_HF  = 0x02
, MAD_VF  = 0x01
, MAD_RGB = 0x00
};

enum colmod_t
{ RGB444_12bit = 0x33
, RGB565_2BYTE = 0x55
, RGB888_3BYTE = 0x66
};

uint8_t getMadCtl(uint8_t r)  {
  static constexpr uint8_t madctl_table[] = {
            MAD_MX|MAD_MH              ,
    MAD_MV                            ,
                          MAD_MY|MAD_ML,
    MAD_MV|MAD_MX|MAD_MY|MAD_MH|MAD_ML,
            MAD_MX|MAD_MH|MAD_MY|MAD_ML,
    MAD_MV|MAD_MX|MAD_MH              ,
                                      0,
    MAD_MV|              MAD_MY|MAD_ML,
  };
  return madctl_table[r];
}

uint8_t getColMod(uint8_t bpp) { return (bpp > 16) ? RGB888_3BYTE : RGB565_2BYTE; }

ST7796S::ST7796S(int width, int height, int colstart, int rowstart, bool eightbitcolor, bool usebgr, bool invert_colors)
    : colstart_(colstart),
      rowstart_(rowstart),
      eightbitcolor_(eightbitcolor),
      usebgr_(usebgr),
      invert_colors_(invert_colors),
      width_(width),
      height_(height) {}

void ST7796S::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST7796S...");
  this->spi_setup();

  this->dc_pin_->setup();  // OUTPUT
  this->cs_->setup();      // OUTPUT

  this->dc_pin_->digital_write(true);
  this->cs_->digital_write(true);

  this->init_reset_();
  delay(100);  // NOLINT

  ESP_LOGD(TAG, "  START");
  dump_config();
  ESP_LOGD(TAG, "  END");

  display_init_(BCMD);

  uint8_t data = 0;

  data = RGB565_2BYTE;
  sendcommand_(ST77XX_COLMOD, &data, 1);

  data = MAD_MX|MAD_MH|MAD_RGB;
  sendcommand_(ST77XX_MADCTL, &data, 1);

  if (this->invert_colors_)
    sendcommand_(ST77XX_INVON, nullptr, 0);

  ESP_LOGD(TAG, "Buffer length: %i", this->get_buffer_length());

  this->init_internal_(this->get_buffer_length());
  memset(this->buffer_, 0x00, this->get_buffer_length());

  backlight_(true);
}

void ST7796S::update() {
  this->do_update_();
  this->write_display_data_();
}

int ST7796S::get_height_internal() { return height_; }

int ST7796S::get_width_internal() { return width_; }

size_t ST7796S::get_buffer_length() {
  if (this->eightbitcolor_) {
    return size_t(this->get_width_internal()) * size_t(this->get_height_internal());
  }
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * 2;
}

void HOT ST7796S::draw_absolute_pixel_internal(int x, int y, Color color) {
  // if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
  //   return;

  if (this->eightbitcolor_) {
    const uint32_t color332 = display::ColorUtil::color_to_332(color);
    uint16_t pos = (x + y * this->get_width_internal());
    this->buffer_[pos] = color332;
  } else {
    const uint32_t color565 = display::ColorUtil::color_to_565(color);
    uint16_t pos = (x + y * this->get_width_internal()) * 2;
    this->buffer_[pos++] = (color565 >> 8) & 0xff;
    this->buffer_[pos] = color565 & 0xff;
  }
}

void ST7796S::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}

void ST7796S::backlight_(bool onoff) {
  if (this->backlight_pin_ != nullptr) {
    this->backlight_pin_->setup();
    this->backlight_pin_->digital_write(onoff);
  }
}

void ST7796S::display_init_(const uint8_t *addr) {
  uint8_t num_commands, cmd, num_args;
  uint16_t ms;

  num_commands = progmem_read_byte(addr++);  // Number of commands to follow
  while (num_commands--) {                   // For each command...
    cmd = progmem_read_byte(addr++);         // Read command
    num_args = progmem_read_byte(addr++);    // Number of args to follow
    ms = num_args & ST_CMD_DELAY;            // If hibit set, delay follows args
    num_args &= ~ST_CMD_DELAY;               // Mask out delay bit
    this->sendcommand_(cmd, addr, num_args);
    addr += num_args;

    if (ms) {
      ms = progmem_read_byte(addr++);  // Read post-command delay time (ms)
      if (ms == 255)
        ms = 500;  // If 255, delay for 500 ms
      delay(ms);
    }
  }
}

void ST7796S::dump_config() {
  LOG_DISPLAY("", "ST7796S", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGD(TAG, "  Buffer Size: %zu", this->get_buffer_length());
  ESP_LOGD(TAG, "  Height: %d", this->height_);
  ESP_LOGD(TAG, "  Width: %d", this->width_);
  ESP_LOGD(TAG, "  ColStart: %d", this->colstart_);
  ESP_LOGD(TAG, "  RowStart: %d", this->rowstart_);
  LOG_UPDATE_INTERVAL(this);
}

void HOT ST7796S::writecommand_(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void HOT ST7796S::writedata_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void HOT ST7796S::sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->writecommand_(cmd);
  this->senddata_(data_bytes, num_data_bytes);
}

void HOT ST7796S::senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->dc_pin_->digital_write(true);  // pull DC high to indicate data
  this->cs_->digital_write(false);
  this->enable();
  for (uint8_t i = 0; i < num_data_bytes; i++) {
    this->write_byte(progmem_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->cs_->digital_write(true);
  this->disable();
}

void HOT ST7796S::write_display_data_() {
  uint16_t offsetx = colstart_;
  uint16_t offsety = rowstart_;

  uint16_t x1 = offsetx;
  uint16_t x2 = x1 + get_width_internal() - 1;
  uint16_t y1 = offsety;
  uint16_t y2 = y1 + get_height_internal() - 1;

  this->enable();

  // set column(x) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_CASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(x1, x2);

  // set Page(y) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(y1, y2);

  //  Memory Write
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RAMWR);
  this->dc_pin_->digital_write(true);

  if (this->eightbitcolor_) {
    for (size_t line = 0; line < this->get_buffer_length(); line = line + this->get_width_internal()) {
      for (int index = 0; index < this->get_width_internal(); ++index) {
        auto color332 = display::ColorUtil::to_color(this->buffer_[index + line], display::ColorOrder::COLOR_ORDER_RGB,
                                                     display::ColorBitness::COLOR_BITNESS_332, true);

        auto color = display::ColorUtil::color_to_565(color332);

        this->write_byte((color >> 8) & 0xff);
        this->write_byte(color & 0xff);
      }
    }
  } else {
    this->write_array(this->buffer_, this->get_buffer_length());
  }
  this->disable();
}

void ST7796S::spi_master_write_addr_(uint16_t addr1, uint16_t addr2) {
  static uint8_t byte[4];
  byte[0] = (addr1 >> 8) & 0xFF;
  byte[1] = addr1 & 0xFF;
  byte[2] = (addr2 >> 8) & 0xFF;
  byte[3] = addr2 & 0xFF;

  this->dc_pin_->digital_write(true);
  this->write_array(byte, 4);
}

void ST7796S::spi_master_write_color_(uint16_t color, uint16_t size) {
  static uint8_t byte[1024];
  int index = 0;
  for (int i = 0; i < size; i++) {
    byte[index++] = (color >> 8) & 0xFF;
    byte[index++] = color & 0xFF;
  }

  this->dc_pin_->digital_write(true);
  return write_array(byte, size * 2);
}

}  // namespace st7796s
}  // namespace esphome
