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

#include <stdint.h>
#include "lcd.h"

static uint8_t *video_framebuffer;

void    video_init(uint32_t *framebuffer) {
  video_framebuffer = (uint8_t*) framebuffer;
}

int video_offset_x = 0, video_offset_y = 0;

// https://github.com/evansm7/umac/pull/16/commits/d90f36714560389c47107c3fc3b3463a5ca09c14
// read mouse position directly from emulator memory:
// x = RAM_RD16(0x82a)
// y = RAM_RD16(0x828)
#include "machw.h"

extern int mouse_mode, cursor_x, cursor_y;

void video_update() {
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
}

