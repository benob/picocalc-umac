/* Keyboard and mouse input handling:
 *
 * Modified to use picocalc keyboard, press right shift to enter mouse mode, space for slow movement, enter for button press
 *
 * Copyright 2025 Benob
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

#include "keyboard.h"
#include "kbd.h"

int cursor_x = 0;
int cursor_y = 0;
int cursor_button = 0;
int mouse_mode = 1;

static int slow = 0, mouse_delta_x = 0, mouse_delta_y = 0;
//static int left_shift_pressed = 0;
//static int right_shift_pressed = 0;

void hid_app_task(void)
{
  input_event_t event = keyboard_poll();
  if (/*!left_shift_pressed &&*/ event.code == KEY_RSHIFT && event.state == KEY_STATE_PRESSED) mouse_mode = 1 - mouse_mode;
  else if (event.code != 0) {
    /*if (event.code == KEY_LSHIFT) {
      if (event.state == KEY_STATE_PRESSED) left_shift_pressed = 1;
      else if (event.state == KEY_STATE_RELEASED) left_shift_pressed = 0;
    }
    if (event.code == KEY_RSHIFT) {
      if (event.state == KEY_STATE_PRESSED) right_shift_pressed = 1;
      else if (event.state == KEY_STATE_RELEASED) right_shift_pressed = 0;
    }*/
    if (mouse_mode) {
      if (event.state == KEY_STATE_PRESSED) {
        switch (event.code) {
          case KEY_LEFT: mouse_delta_x = -1; break;
          case KEY_RIGHT: mouse_delta_x = 1; break;
          case KEY_UP: mouse_delta_y = -1; break;
          case KEY_DOWN: mouse_delta_y = 1; break;
          case KEY_ENTER: cursor_button = 1; break;
          case KEY_SPACE: break;
          default:
            kbd_queue_push(event.code, event.state == KEY_STATE_PRESSED);
            break;
        }
      } else if (event.state == KEY_STATE_RELEASED) {
        switch (event.code) {
          case KEY_LEFT: 
          case KEY_RIGHT: mouse_delta_x = 0; break;
          case KEY_UP: 
          case KEY_DOWN: mouse_delta_y = 0; break;
          case KEY_ENTER: cursor_button = 0; break;
          case KEY_SPACE: slow = 1 - slow; break;
          default:
            kbd_queue_push(event.code, event.state == KEY_STATE_PRESSED);
            break;
        }
      }
    } else {
      if (event.state == KEY_STATE_PRESSED || event.state == KEY_STATE_RELEASED) kbd_queue_push(event.code, event.state == KEY_STATE_PRESSED);
    }
  }
  cursor_x += mouse_delta_x * (slow ? 1 : 2);
  cursor_y += mouse_delta_y * (slow ? 1 : 2);
}
//
