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
static const uint8_t umac_disc[] = {
#include "umac-disc.h"
};
static const uint8_t umac_rom[] = {
#include "umac-rom.h"
};

static uint8_t umac_ram[RAM_SIZE];

////////////////////////////////////////////////////////////////////////////////

static void     io_init()
{
        gpio_init(GPIO_LED_PIN);
        gpio_set_dir(GPIO_LED_PIN, GPIO_OUT);
}

/*static void     poll_led_etc()
{
        static int led_on = 0;
        static absolute_time_t last = 0;
        absolute_time_t now = get_absolute_time();

        if (absolute_time_diff_us(last, now) > 500*1000) {
                last = now;

                led_on ^= 1;
                gpio_put(GPIO_LED_PIN, led_on);
        }
}*/

static int umac_cursor_x = 0;
static int umac_cursor_y = 0;
static int umac_cursor_button = 0;

static void     poll_umac()
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
static int      disc_do_read(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
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

static int      disc_do_write(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
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

static FIL discfp;
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

static void     disc_setup(disc_descr_t discs[DISC_NUM_DRIVES])
{
#if USE_SD
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
    goto no_sd;
  }
  
  char* disc0_name = "umac0.img";
  result = f_open(&discfp, disc0_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
  if (result != FR_OK) {
    printf("f_open: %s (%d)\n", fs_error_strings[result], result);
    goto no_sd;
  }

  discs[0].base = 0; // Means use R/W ops
  discs[0].read_only = 0;
  discs[0].size = f_size(&discfp);
  discs[0].op_ctx = &discfp;
  discs[0].op_read = disc_do_read;
  discs[0].op_write = disc_do_write;

        /*char *disc0_name;
        const char *disc0_ro_name = "umac0ro.img";
        const char *disc0_pattern = "umac0*.img";

        printf("Starting SPI/FatFS:\n");
        set_spi_dma_irq_channel(true, false);
        sd_card_t *pSD = sd_get_by_num(0);
        //FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        FRESULT fr = f_mount(&pSD->fatfs, "", 1);
        setup_default_uart();
        printf("  mount: %d\n", fr);
        if (fr != FR_OK) {
                printf("  error mounting disc: %s (%d)\n", FRESULT_str(fr), fr);
                goto no_sd;
        }

        DIR di = {0};
        FILINFO fi = {0};
        fr = f_findfirst(&di, &fi, "/", disc0_pattern);
        if (fr != FR_OK) {
                printf("  Can't find images %s: %s (%d)\n", disc0_pattern, FRESULT_str(fr), fr);
                goto no_sd;
        }
        disc0_name = fi.fname;
        f_closedir(&di);

        int read_only = !strcmp(disc0_name, disc0_ro_name);
        printf("  Opening %s (R%c)\n", disc0_name, read_only ? 'O' : 'W');

        fr = f_open(&discfp, disc0_name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
        if (fr != FR_OK && fr != FR_EXIST) {
                printf("  *** Can't open %s: %s (%d)!\n", disc0_name, FRESULT_str(fr), fr);
                goto no_sd;
        } else {
                printf("  Opened, size 0x%x\n", f_size(&discfp));
                if (read_only)
                        printf("  (disc is read-only)\n");
                discs[0].base = 0; // Means use R/W ops
                discs[0].read_only = read_only;
                discs[0].size = f_size(&discfp);
                discs[0].op_ctx = &discfp;
                discs[0].op_read = disc_do_read;
                discs[0].op_write = disc_do_write;
        }*/

        /* FIXME: Other files can be stored on SD too, such as logging
         * and NVRAM storage.
         *
         * We could also implement a menu here to select an image,
         * writing text to the framebuffer and checking kbd_queue_*()
         * for user input.
         */
        printf("loaded SD (size=%ld)\n", discs[0].size);
        return;

no_sd:
#endif
        /* If we don't find (or look for) an SD-based image, attempt
         * to use in-flash disc image:
         */
        discs[0].base = (void *)umac_disc;
        discs[0].read_only = 1;
        discs[0].size = sizeof(umac_disc);
        printf("using flash img\n");
}

static void     core1_main()
{
        disc_descr_t discs[DISC_NUM_DRIVES] = {0};

        printf("Core 1 started\n");
        disc_setup(discs);

        umac_init(umac_ram, (void *)umac_rom, discs);
        /* Video runs on core 1, i.e. IRQs/DMA are unaffected by
         * core 0's USB activity.
         */
        video_init((uint32_t *)(umac_ram + umac_get_fb_offset()));

        printf("Enjoyable Mac times now begin:\n\n");

        while (true) {
                poll_umac();
        }
}

int     main()
{
  // overclock at 210MHz
  //set_sys_clock_khz(210*1000, true);
  //clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, 210 * 1000, 210 * 1000);

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
                //poll_led_etc();
                video_update();
	}

	return 0;
}

