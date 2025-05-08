#include <stdarg.h>
#include <stdio.h>
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "lcd.h"
#include "font.h"

#define LCD_SCK 10
#define LCD_TX  11
#define LCD_RX  12
#define LCD_CS  13
#define LCD_DC  14
#define LCD_RST 15

static int lcd_write_spi(void *buf, size_t len) {
  return spi_write_blocking(spi1, buf, len);
}

static inline void lcd_write_spi_dc(void *buf, size_t len, int dc) {
  gpio_put(LCD_DC, dc);
  lcd_write_spi(buf, len);
}

#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__}) / sizeof(int))
static int lcd_write_reg_num(int len, ...) {
  u8 buf[128];
  va_list args;
  int i;

  gpio_put(LCD_DC, 0);
  va_start(args, len);
  *buf = (u8)va_arg(args, unsigned int);
  spi_write_blocking(spi1, buf, sizeof(u8));
  len--;

  gpio_put(LCD_DC, 1);
  if (len > 0) {
    for (i = 0; i < len; i++) buf[i] = (u8)va_arg(args, unsigned int);
    spi_write_blocking(spi1, buf, len);
  }
  va_end(args);

  return 0;
}
#define lcd_write_reg(...) \
    lcd_write_reg_num(NUMARGS(__VA_ARGS__), __VA_ARGS__)

#define REGION_READ 0
#define REGION_WRITE 1

static void lcd_set_region(int x1, int y1, int x2, int y2, int rw) {
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x2A, (x1 >> 8), (x1 & 0xFF), (x2 >> 8), (x2 & 0xFF));
  lcd_write_reg(0x2B, (y1 >> 8), (y1 & 0xFF), (y2 >> 8), (y2 & 0xFF));
  if (rw) lcd_write_reg(0x2C);
  else lcd_write_reg(0x2E);
}

static int lcd_write_data(u8* buf, int len) {
  return lcd_write_spi(buf, len);
}

void lcd_draw(u16* pixels, int x, int y, int width, int height) {
  y %= MEM_HEIGHT;
  lcd_set_region(x, y, x + width - 1, y + height - 1, REGION_WRITE);

  //lcd_write_data((u8*) pixels, width * height * 2);
  spi_set_format(spi1, 16, 0, 0, SPI_MSB_FIRST);
  spi_write16_blocking(spi1, pixels, width * height);
  spi_set_format(spi1, 8, 0, 0, SPI_MSB_FIRST);
  gpio_put(LCD_CS, 1);
}

#define USE_16BIT
#define USE_BUFFER
#define USE_DMA

void lcd_fill(u16 color, int x, int y, int width, int height) {
  y %= MEM_HEIGHT;
  lcd_set_region(x, y, x + width - 1, y + height - 1, REGION_WRITE);

  gpio_put(LCD_DC, 1);
  spi_set_format(spi1, 16, 0, 0, SPI_MSB_FIRST);

  int dma_chan = dma_claim_unused_channel(true);
  dma_channel_config config = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
  channel_config_set_read_increment(&config, true);
  channel_config_set_write_increment(&config, false);
  channel_config_set_dreq(&config, DREQ_SPI1_TX);

  u16 buf[256];
  int remaining = width * height;
  int chunk_size = remaining < 256 ? remaining : 256;
  for (int i = 0; i < chunk_size; i++) buf[i] = color;

  while (remaining > chunk_size) {
    dma_channel_configure( dma_chan, &config, &spi_get_hw(spi1)->dr, buf, chunk_size, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    remaining -= chunk_size;
  }
  dma_channel_unclaim(dma_chan);

  //u16 color2 = (color >> 8) | (color << 8);
  //for (int i = 0; i < width * height; i++) spi_write8_blocking(spi1, &color2, 2);

  /*u16 buf[256];
  int remaining = width * height;
  int chunk_size = remaining < 256 ? remaining : 256;
  for (int i = 0; i < chunk_size; i++) buf[i] = color;

  while (remaining > chunk_size) {
    spi_write16_blocking(spi1, buf, chunk_size);
    remaining -= chunk_size;
  }
  spi_write16_blocking(spi1, buf, remaining);*/

  spi_set_format(spi1, 8, 0, 0, SPI_MSB_FIRST);
  gpio_put(LCD_CS, 1);
}

int lcd_clear() {
  lcd_fill(0, 0, 0, WIDTH, MEM_HEIGHT);
}

void lcd_scroll(int lines) {
  lines %= MEM_HEIGHT;
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x37, (lines >> 8), (lines & 0xFF));
  gpio_put(LCD_CS, 1);
}

void lcd_setup_scrolling(int top_fixed_lines, int bottom_fixed_lines) {
  int vertical_scrolling_area = HEIGHT - (top_fixed_lines + bottom_fixed_lines);
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x33, (top_fixed_lines >> 8), (top_fixed_lines & 0xFF), (vertical_scrolling_area >> 8), 
    (vertical_scrolling_area & 0xFF), (bottom_fixed_lines >> 8), (bottom_fixed_lines & 0xff));
  gpio_put(LCD_CS, 1);
}

font_t font = {
  .glyphs = (u8*)GLYPHS,
  .glyph_width = GLYPH_WIDTH,
  .glyph_height = GLYPH_HEIGHT,
};


u16 glyph_buf[GLYPH_WIDTH * GLYPH_HEIGHT] __attribute__((aligned(4))); 

void lcd_draw_char(int x, int y, u16 fg, u16 bg, char c) {
  int offset = ((u8)c) * GLYPH_HEIGHT;
  for (int j = 0; j < GLYPH_HEIGHT; j++) {
    for (int i = 0; i < GLYPH_WIDTH; i++) {
      int mask = 1 << i;
      glyph_buf[i + j * GLYPH_WIDTH] = (font.glyphs[offset] & mask) ? fg : bg;
    }
    offset++;
  }

  lcd_draw(glyph_buf, x, y, GLYPH_WIDTH, GLYPH_HEIGHT);
}

void lcd_draw_text(int x, int y, u16 fg, u16 bg, const char* text) {
  //if (y <= -font.glyph_height || y >= HEIGHT) return;
  while(*text) {
    lcd_draw_char(x, y, fg, bg, *text);
    x += font.glyph_width;
    if (x > WIDTH) return;
    text ++;
  }
}

void lcd_printf(int x, int y, u16 fg, u16 bg, const char* format, ...) {
  char buffer[256];
  va_list list;
  va_start(list, format);
  int result = vsnprintf(buffer, 256, format, list);
  if (result > -1) {
    lcd_draw_text(x, y, fg, bg, buffer);
  }
}


#define LCD_SPI_SPEED   (105 * 1e6)
#define PORTCLR             1
#define PORTSET             2
#define PORTINV             3
#define LAT                 4
#define LATCLR              5
#define LATSET              6
#define LATINV              7
#define ODC                 8
#define ODCCLR              9
#define ODCSET              10
#define CNPU                12
#define CNPUCLR             13
#define CNPUSET             14
#define CNPUINV             15
#define CNPD                16
#define CNPDCLR             17
#define CNPDSET             18
#define ANSELCLR            -7
#define ANSELSET            -6
#define ANSELINV            -5
#define TRIS                -4
#define TRISCLR             -3
#define TRISSET             -2

static void pin_set_bit(int pin, unsigned int offset) {
  switch (offset) {
    case LATCLR:
      gpio_set_pulls(pin, false, false);
      gpio_pull_down(pin);
      gpio_put(pin, 0);
      return;
    case LATSET:
      gpio_set_pulls(pin, false, false);
      gpio_pull_up(pin);
      gpio_put(pin, 1);
      return;
    case LATINV:
      gpio_xor_mask(1 << pin);
      return;
    case TRISSET:
      gpio_set_dir(pin, GPIO_IN);
      sleep_us(2);
      return;
    case TRISCLR:
      gpio_set_dir(pin, GPIO_OUT);
      gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
      sleep_us(2);
      return;
    case CNPUSET:
      gpio_set_pulls(pin, true, false);
      return;
    case CNPDSET:
      gpio_set_pulls(pin, false, true);
      return;
    case CNPUCLR:
    case CNPDCLR:
      gpio_set_pulls(pin, false, false);
      return;
    case ODCCLR:
      gpio_set_dir(pin, GPIO_OUT);
      gpio_put(pin, 0);
      sleep_us(2);
      return;
    case ODCSET:
      gpio_set_pulls(pin, true, false);
      gpio_set_dir(pin, GPIO_IN);
      sleep_us(2);
      return;
    case ANSELCLR:
      gpio_set_function(pin, GPIO_FUNC_SIO);
      gpio_set_dir(pin, GPIO_IN);
      return;
    default:
      printf("unknown pin_set_bit command\n");
      break;
  }
}

void lcd_init() {
  // Init GPIO
  gpio_init(LCD_SCK);
  gpio_init(LCD_TX);
  gpio_init(LCD_RX);
  gpio_init(LCD_CS);
  gpio_init(LCD_DC);
  gpio_init(LCD_RST);

  gpio_set_dir(LCD_SCK, GPIO_OUT);
  gpio_set_dir(LCD_TX, GPIO_OUT);
  //gpio_set_dir(LCD_RX, GPIO_IN);
  gpio_set_dir(LCD_CS, GPIO_OUT);
  gpio_set_dir(LCD_DC, GPIO_OUT);
  gpio_set_dir(LCD_RST, GPIO_OUT);

  // Init SPI
  spi_init(spi1, LCD_SPI_SPEED);
  gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
  gpio_set_function(LCD_TX, GPIO_FUNC_SPI);
  gpio_set_function(LCD_RX, GPIO_FUNC_SPI);
  gpio_set_input_hysteresis_enabled(LCD_RX, true);

  gpio_put(LCD_CS, 1);
  gpio_put(LCD_RST, 1);

  // Reset controller
  pin_set_bit(LCD_RST, LATSET);
  sleep_ms(10);
  pin_set_bit(LCD_RST, LATCLR);
  sleep_ms(10);
  pin_set_bit(LCD_RST, LATSET);
  sleep_ms(200);

  // Setup LCD
  gpio_put(LCD_CS, 0);
  // Positive Gamma Control
  lcd_write_reg(0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F);

  // Negative Gamma Control
  lcd_write_reg(0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F);

  lcd_write_reg(0xC0, 0x17, 0x15);          // Power Control 1
  lcd_write_reg(0xC1, 0x41);                // Power Control 2
  lcd_write_reg(0xC5, 0x00, 0x12, 0x80);    // VCOM Control
  lcd_write_reg(0x36, 0x48);                // Memory Access Control (0x48=BGR, 0x40=RGB)
  lcd_write_reg(0x3A, 0x55);                // Pixel Interface Format  16 bit colour for SPI
  lcd_write_reg(0xB0, 0x00);                // Interface Mode Control

  // Frame Rate Control
  //lcd_write_reg(0xB1, 0xA0);
  lcd_write_reg(0xB1, 0xD0, 0x11);          // 60Hz
  //lcd_write_reg(0xB1, 0xD0, 0x14);          // 90Hz
  lcd_write_reg(0x21);                      // Invert colors on

  lcd_write_reg(0xB4, 0x02);                // Display Inversion Control
  lcd_write_reg(0xB6, 0x02, 0x02, 0x3B);    // Display Function Control
  lcd_write_reg(0xB7, 0xC6);                // Entry Mode Set
  lcd_write_reg(0xE9, 0x00);
  lcd_write_reg(0xF7, 0xA9, 0x51, 0x2C, 0x82);  // Adjust Control 3
  lcd_write_reg(0x11);                      // Exit Sleep
  //sleep_ms(120);
  //lcd_write_reg(0x29);                      // Display on
  sleep_ms(120);
  gpio_put(LCD_CS, 1);
}

void lcd_blank() {
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x10);                      // Enter Sleep
  gpio_put(LCD_CS, 1);
}

void lcd_unblank() {
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x11);                      // Exit Sleep
  gpio_put(LCD_CS, 1);
}

void lcd_on() {
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x29);                      // Display on
  gpio_put(LCD_CS, 1);
}

void lcd_off() {
  gpio_put(LCD_CS, 0);
  lcd_write_reg(0x29);                      // Display off
  gpio_put(LCD_CS, 1);
}

