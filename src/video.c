/* Video output:
 *
 * Modified to use picocalc LCD, adds a red mouse indicator, supports a virtual screen
 *
 * Copyright 2025 Benob
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
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "video.h"

#include "lcd_3bit.h"
#include "font.h"

static uint8_t *video_framebuffer = NULL;

void    video_init(uint32_t *framebuffer) {
  video_framebuffer = (uint8_t*) framebuffer;
  memset(video_framebuffer, DISP_WIDTH * DISP_HEIGHT / 8, 0);
}

int video_offset_x = 0, video_offset_y = 0;

// https://github.com/evansm7/umac/pull/16/commits/d90f36714560389c47107c3fc3b3463a5ca09c14
// read mouse position directly from emulator memory:
// x = RAM_RD16(0x82a)
// y = RAM_RD16(0x828)
#include "machw.h"

extern int mouse_mode, cursor_x, cursor_y;

/*void video_update_rgb565() {

  if (video_framebuffer == NULL) return;

  int mouse_x = RAM_RD16(0x82a);
  int mouse_y = RAM_RD16(0x828);
  // adjust display position
  if (mouse_x >= 0 && mouse_x < DISP_WIDTH && mouse_y >= 0 && mouse_y < DISP_HEIGHT) {
    while (mouse_x < video_offset_x && video_offset_x > 0) video_offset_x--;
    while (mouse_x > video_offset_x + 320 && video_offset_x < DISP_WIDTH - 320) video_offset_x++;
    while (mouse_y < video_offset_y && video_offset_y > 0) video_offset_y--;
    while (mouse_y > video_offset_y + 320 && video_offset_y < DISP_HEIGHT - 320) video_offset_y++;
  }

  // draw row by row
  uint16_t row[320];
  for (int y = 0; y < 320; y++) {
    uint16_t* fb_out = row;
    for (int x = 0; x < 320; x += 16) {
      uint8_t plo = video_framebuffer[(x + video_offset_x)/8 + ((y + video_offset_y) * DISP_WIDTH/8) + 0];
      uint8_t phi = video_framebuffer[(x + video_offset_x)/8 + ((y + video_offset_y) * DISP_WIDTH/8) + 1];
      for (int i = 0; i < 8; i++) {
        *fb_out++ = (plo & (0x80 >> i)) ? 0 : 0xffff;
      }
      for (int i = 0; i < 8; i++) {
        *fb_out++ = (phi & (0x80 >> i)) ? 0 : 0xffff;
      }
    }       
    lcd_draw(row, 0, y, 320, 1);
  }

  // mouse indicator
  //if (mouse_mode) lcd_draw_char(4, 319 - 12, 0, RGB(255, 0, 0), 'm');
  //lcd_printf(4, 319 - 12, 0, RGB(255, 0, 0), "x=%d y=%d\n", mouse_x, mouse_y);
}*/

void hid_app_task(void);

void video_update() {

  if (video_framebuffer == NULL) return;

  int mouse_x = RAM_RD16(0x82a);
  int mouse_y = RAM_RD16(0x828);
  // adjust display position
  if (mouse_x >= 0 && mouse_x < DISP_WIDTH && mouse_y >= 0 && mouse_y < DISP_HEIGHT) {
    while (mouse_x < video_offset_x && video_offset_x > 0) video_offset_x--;
    while (mouse_x > video_offset_x + 320 && video_offset_x < DISP_WIDTH - 320) video_offset_x++;
    while (mouse_y < video_offset_y && video_offset_y > 0) video_offset_y--;
    while (mouse_y > video_offset_y + 320 && video_offset_y < DISP_HEIGHT - 320) video_offset_y++;
  }

  // draw row by row
  uint8_t row[160];
  for (int y = 0; y < 320; y++) {
    uint8_t* fb_out = row;
    for (int x = 0; x < 320; x += 16) {
      uint8_t plo = video_framebuffer[(x + video_offset_x)/8 + ((y + video_offset_y) * DISP_WIDTH/8) + 0];
      uint8_t phi = video_framebuffer[(x + video_offset_x)/8 + ((y + video_offset_y) * DISP_WIDTH/8) + 1];
      for (int i = 0; i < 8; i+=2) {
        *fb_out++ = ((plo & (0x80 >> i)) ? 0 : 56) | ((plo & (0x80 >> (i + 1))? 0 : 7));
      }
      for (int i = 0; i < 8; i+=2) {
        *fb_out++ = ((phi & (0x80 >> i)) ? 0 : 56) | ((phi & (0x80 >> (i + 1))? 0 : 7));
      }
    }       
    lcd_draw(row, 0, y, 320, 1);
    if (y % 80 == 0) hid_app_task(); // check for key presses more often
  }

  // mouse indicator
  //if (mouse_mode) lcd_draw_char(4, 319 - 12, 0, RGB(255, 0, 0), 'm');
  //lcd_printf(4, 319 - 12, 0, RGB(255, 0, 0), "x=%d y=%d\n", mouse_x, mouse_y);
}

void fb_fill_rect(int x, int y, int width, int height, uint8_t color) {
  if (video_framebuffer == NULL) return;

  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > DISP_WIDTH) width = DISP_WIDTH - x;
  if (y + height > DISP_HEIGHT) height = DISP_HEIGHT - y;

  for (int j = y; j < y + height; ++j) {
    for (int i = 0; i < width; ++i) {
      int offset = j * DISP_WIDTH + i;
      int byte = offset >> 3;
      int bit = offset & 7;
      if (color) video_framebuffer[byte] |= bit;
      else video_framebuffer[byte] &= ~bit;
    }
  }
}

void fb_draw_char(int x, int y, uint8_t color, char c) {
  int offset = ((uint8_t)c) * GLYPH_HEIGHT;
  for (int j = 0; j < GLYPH_HEIGHT; j++) {
    for (int i = 0; i < GLYPH_WIDTH; i++) {
      int mask = 1 << i;
      if (y + j >= 0 && y + j < DISP_HEIGHT && x + i >= 0 && x + i < DISP_WIDTH) {
        if (font.glyphs[offset] & mask) {
          int fb_offset = (y + j) * DISP_WIDTH + (x + i);
          int byte = fb_offset >> 3;
          int bit = fb_offset & 7;
          if (color) video_framebuffer[byte] |= bit;
          else video_framebuffer[byte] &= ~bit;
        }
      }
    }
    offset++;
  }
}

void fb_draw_text(int x, int y, uint8_t color, const char* text) {
  while(*text) {
    fb_draw_char(x, y, color, *text);
    x += font.glyph_width;
    if (x > DISP_WIDTH) return;
    text ++;
  }
}

void fb_printf(int x, int y, uint8_t color, const char* format, ...) {
  char buffer[256];
  va_list list;
  va_start(list, format);
  int result = vsnprintf(buffer, 256, format, list);
  if (result > -1) {
    fb_draw_text(x, y, color, buffer);
  }
}

