/* pico-umac
 *
 * Main loop to initialise umac, and run main event loop (piping
 * keyboard/mouse events in).
 *
 * Copyright 2024 Matt Evans
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#ifdef USE_PSRAM
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/xip.h"
#endif

#include "hw.h"
#include "video.h"
#include "kbd.h"

//#include "bsp/rp2040/board.h"
//#include "tusb.h"

#include "keyboard.h"
#include "lcd_3bit.h"

#include "umac.h"

#if USE_SD
//#include "f_util.h"
//#include "ff.h"
//#include "rtc.h"
//#include "hw_config.h"
#include "tf_card.h"
#include "fatfs/ff.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Imports and data

extern void     hid_app_task(void);
extern int cursor_x;
extern int cursor_y;
extern int cursor_button;

// Mac binary data:  disc and ROM images
#ifndef USE_SD
static const uint8_t umac_disc[] = {
#include "umac-disc.h"
};
#endif

static const uint8_t umac_rom[] = {
#include "umac-rom.h"
};

#ifdef USE_PSRAM
static uint8_t* umac_ram = (uint8_t*) 0x11000000; // PSRAM xip base
#else
static uint8_t umac_ram[RAM_SIZE];
#endif

////////////////////////////////////////////////////////////////////////////////

static void io_init()
{
#ifdef USE_PSRAM
  gpio_set_function(PSRAM_PIN, GPIO_FUNC_XIP_CS1); // CS for PSRAM
  xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
  memset(umac_ram, 0, RAM_SIZE);
#endif
}

static int umac_cursor_x = 0;
static int umac_cursor_y = 0;
static int umac_cursor_button = 0;

static void poll_umac()
{
  static absolute_time_t last_1hz = 0;
  static absolute_time_t last_vsync = 0;
  absolute_time_t now = get_absolute_time();

  umac_loop();

  int64_t p_1hz = absolute_time_diff_us(last_1hz, now);
  int64_t p_vsync = absolute_time_diff_us(last_vsync, now);
  if (p_vsync >= 16667) {
    /* FIXME: Trigger this off actual vsync */
    umac_vsync_event();
    last_vsync = now;
  }
  if (p_1hz >= 1000000) {
    umac_1hz_event();
    last_1hz = now;
  }

  int update = 0;
  int dx = 0;
  int dy = 0;
  int b = umac_cursor_button;
  if (cursor_x != umac_cursor_x) {
    dx = cursor_x - umac_cursor_x;
    umac_cursor_x = cursor_x;
    update = 1;
  }
  if (cursor_y != umac_cursor_y) {
    dy = cursor_y - umac_cursor_y;
    umac_cursor_y = cursor_y;
    update = 1;
  }
  if (cursor_button != umac_cursor_button) {
    b = cursor_button;
    umac_cursor_button = cursor_button;
    update = 1;
  }
  if (update) {
    umac_mouse(dx, -dy, b);
  }

  if (!kbd_queue_empty()) {
    uint16_t k = kbd_queue_pop();
    umac_kbd_event(k & 0xff, !!(k & 0x8000));
  }
}

#if USE_SD
static int disc_do_read(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
  printf("sd read %p %d %d\n", data, offset, len);
  FIL *fp = (FIL *)ctx;
  f_lseek(fp, offset);
  unsigned int did_read = 0;
  FRESULT fr = f_read(fp, data, len, &did_read);
  if (fr != FR_OK || len != did_read) {
    printf("disc: f_read returned %d, read %u (of %u)\n", fr, did_read, len);
    return -1;
  }
  return 0;
}

static int disc_do_write(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
  printf("sd write %p %d %d\n", data, offset, len);
  FIL *fp = (FIL *)ctx;
  f_lseek(fp, offset);
  unsigned int did_write = 0;
  FRESULT fr = f_write(fp, data, len, &did_write);
  if (fr != FR_OK || len != did_write) {
    printf("disc: f_write returned %d, read %u (of %u)\n", fr, did_write, len);
    return -1;
  }
  return 0;
}

static FIL discfp, discfp2;
static FATFS fatfs;
static char* fs_error_strings[20] = {
  "Succeeded",
  "A hard error occurred in the low level disk I/O layer",
  "Assertion failed",
  "The physical drive cannot work",
  "Could not find the file",
  "Could not find the path",
  "The path name format is invalid",
  "Access denied due to prohibited access or directory full",
  "Access denied due to prohibited access",
  "The file/directory object is invalid",
  "The physical drive is write protected",
  "The logical drive number is invalid",
  "The volume has no work area",
  "There is no valid FAT volume",
  "The f_mkfs() aborted due to any problem",
  "Could not get a grant to access the volume within defined period",
  "The operation is rejected according to the file sharing policy",
  "LFN working buffer could not be allocated",
  "Number of open files > FF_FS_LOCK",
  "Given parameter is invalid",
};

#endif

static int disc_setup(disc_descr_t discs[DISC_NUM_DRIVES])
{
#if USE_SD
  int line = 0;
  lcd_printf(0, 10 * (line++), 0x6, 0, "loading umac0.img from sdcard");

  pico_fatfs_spi_config_t config = {
    .spi_inst = spi0,
    .clk_slow = CLK_SLOW_DEFAULT,
    .clk_fast = CLK_FAST_DEFAULT,
    .pin_miso = 16,
    .pin_cs = 17,
    .pin_sck = 18,
    .pin_mosi = 19,
    .pullup = true,
  };
  pico_fatfs_set_config(&config);
  bool spi_configured = pico_fatfs_set_config(&config);
  if (!spi_configured) {
    pico_fatfs_config_spi_pio(pio0, 0);  // PIO, sm
  }

  FRESULT result;
  result = f_mount(&fatfs, "", 0);
  if (result != FR_OK) {
    printf("f_mount: %s (%d)\n", fs_error_strings[result], result);
    lcd_printf(0, 10 * (line++), 0x6, 0, "f_mount: %s (%d)", fs_error_strings[result], result);
    sleep_ms(100);
    goto no_sd;
  }

  char* disc0_name = "umac0.img";
  result = f_open(&discfp2, disc0_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
  if (result != FR_OK) {
    printf("f_open: %s (%d)\n", fs_error_strings[result], result);
    lcd_printf(0, 10 * (line++), 0x6, 0, "f_open: %s (%d)", fs_error_strings[result], result);
    sleep_ms(100);
    goto no_sd;
  }

  discs[0].base = 0; // Means use R/W ops
  discs[0].read_only = 0;
  discs[0].size = f_size(&discfp2);
  discs[0].op_ctx = &discfp2;
  discs[0].op_read = disc_do_read;
  discs[0].op_write = disc_do_write;

  lcd_printf(0, 10 * (line++), 0x6, 0, "loading umac1.img from sdcard");
  char* disc1_name = "umac1.img";
  result = f_open(&discfp, disc1_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
  if (result != FR_OK) {
    printf("f_open: %s (%d)\n", fs_error_strings[result], result);
    lcd_printf(0, 10 * (line++), 0x6, 0, "f_open: %s (%d)", fs_error_strings[result], result);
    sleep_ms(100);
    return 1;
  }

  discs[1].base = 0; // Means use R/W ops
  discs[1].read_only = 0;
  discs[1].size = f_size(&discfp);
  discs[1].op_ctx = &discfp;
  discs[1].op_read = disc_do_read;
  discs[1].op_write = disc_do_write;

  printf("loaded SD (size=%ld)\n", discs[0].size);
  return 1;

no_sd:

  lcd_printf(0, 10, 0x6, 0, "no disk found, please insert SD card");
  sleep_ms(100);
  return 0;
#else
  /* If we don't find (or look for) an SD-based image, attempt
   * to use in-flash disc image:
   */
  discs[0].base = (void *)umac_disc;
  discs[0].read_only = 1;
  discs[0].size = sizeof(umac_disc);
  printf("using flash img\n");
  return 1;
#endif
}

static void core1_main()
{
  disc_descr_t discs[DISC_NUM_DRIVES] = {0};

  printf("Core 1 started\n");
  while (!disc_setup(discs));

  umac_init(umac_ram, (void *)umac_rom, discs);

  /* video runs on core 0 */
  video_init((uint32_t *)(umac_ram + umac_get_fb_offset()));
  fb_printf(0, 0, 1, "starging umac");

  printf("Enjoyable Mac times now begin:\n\n");

  while (true) {
    poll_umac();
  }
}

int     main()
{

  /* overclock at 210MHz
  set_sys_clock_khz(210*1000, true);
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, 210 * 1000, 210 * 1000);
  */

  stdio_init_all();

  lcd_init();
  lcd_clear();
  lcd_on();

  keyboard_init();
  io_init();

  multicore_launch_core1(core1_main);

  //printf("Starting, init usb\n");
  //tusb_init();

  /* This happens on core 0: */
  while (true) {
    //hid_app_task();
    video_update();
  }

  return 0;
}

