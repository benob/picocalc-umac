/* HID to Mac keyboard scancode mapping
 *
 * FIXME: This doesn't do capslock (needs to track toggle), and arrow
 * keys don't work.
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
#include "kbd.h"

#include "keyboard.h"

//#include "class/hid/hid.h"
#include "keymap.h"

#define KQ_SIZE         32
#define KQ_MASK         (KQ_SIZE-1)

static uint16_t kbd_queue[KQ_SIZE];
static unsigned int kbd_queue_prod = 0;
static unsigned int kbd_queue_cons = 0;

static bool     kbd_queue_full()
{
        return ((kbd_queue_prod + 1) & KQ_MASK) == kbd_queue_cons;
}


bool            kbd_queue_empty()
{
        return kbd_queue_prod == kbd_queue_cons;
}

/* If empty, return 0, else return a mac keycode in [7:0] and [15] set if a press (else release) */
uint16_t        kbd_queue_pop()
{
        if (kbd_queue_empty())
                return 0;
        uint16_t v = kbd_queue[kbd_queue_cons];
        kbd_queue_cons = (kbd_queue_cons + 1) & KQ_MASK;
        return v;
}

static const uint8_t key_mapping[256] = {
  [KEY_NONE] = 0,

  [KEY_ALT] = MKC_Option,
  [KEY_LSHIFT] = MKC_Shift,
  [KEY_RSHIFT] = MKC_Command,
  [KEY_CONTROL] = MKC_Control,

  [KEY_a] = MKC_A,
  [KEY_b] = MKC_B,
  [KEY_c] = MKC_C,
  [KEY_d] = MKC_D,
  [KEY_e] = MKC_E,
  [KEY_f] = MKC_F,
  [KEY_g] = MKC_G,
  [KEY_h] = MKC_H,
  [KEY_i] = MKC_I,
  [KEY_j] = MKC_J,
  [KEY_k] = MKC_K,
  [KEY_l] = MKC_L,
  [KEY_m] = MKC_M,
  [KEY_n] = MKC_N,
  [KEY_o] = MKC_O,
  [KEY_p] = MKC_P,
  [KEY_q] = MKC_Q,
  [KEY_r] = MKC_R,
  [KEY_s] = MKC_S,
  [KEY_t] = MKC_T,
  [KEY_u] = MKC_U,
  [KEY_v] = MKC_V,
  [KEY_w] = MKC_W,
  [KEY_x] = MKC_X,
  [KEY_y] = MKC_Y,
  [KEY_z] = MKC_Z,

  [KEY_A] = MKC_A,
  [KEY_B] = MKC_B,
  [KEY_C] = MKC_C,
  [KEY_D] = MKC_D,
  [KEY_E] = MKC_E,
  [KEY_F] = MKC_F,
  [KEY_G] = MKC_G,
  [KEY_H] = MKC_H,
  [KEY_I] = MKC_I,
  [KEY_J] = MKC_J,
  [KEY_K] = MKC_K,
  [KEY_L] = MKC_L,
  [KEY_M] = MKC_M,
  [KEY_N] = MKC_N,
  [KEY_O] = MKC_O,
  [KEY_P] = MKC_P,
  [KEY_Q] = MKC_Q,
  [KEY_R] = MKC_R,
  [KEY_S] = MKC_S,
  [KEY_T] = MKC_T,
  [KEY_U] = MKC_U,
  [KEY_V] = MKC_V,
  [KEY_W] = MKC_W,
  [KEY_X] = MKC_X,
  [KEY_Y] = MKC_Y,
  [KEY_Z] = MKC_Z,

  [KEY_SPACE] = MKC_Space,
  [KEY_TILDE] = MKC_Grave,
  [KEY_DOLLAR] = MKC_4,

  [KEY_BACKSPACE] = MKC_BackSpace,
  [KEY_ENTER] = MKC_Return,

  [KEY_0] = MKC_0,
  [KEY_1] = MKC_1,
  [KEY_2] = MKC_2,
  [KEY_3] = MKC_3,
  [KEY_4] = MKC_4,
  [KEY_5] = MKC_5,
  [KEY_6] = MKC_6,
  [KEY_7] = MKC_7,
  [KEY_8] = MKC_8,
  [KEY_9] = MKC_9,

  [KEY_F1] = MKC_F1,
  [KEY_F2] = MKC_F2,
  [KEY_F3] = MKC_F3,
  [KEY_F4] = MKC_F4,
  [KEY_F5] = MKC_F5,
  [KEY_F6] = MKC_F6,
  [KEY_F7] = MKC_F7,
  [KEY_F8] = MKC_F8,
  [KEY_F9] = MKC_F9,
  [KEY_F10] = MKC_F10,

  [KEY_SHARP] = MKC_3,
  [KEY_AT] = MKC_2,
  [KEY_DELETE] = MKC_BackSpace,
  [KEY_END] = MKC_End,
  [KEY_CAPSLOCK] = MKC_CapsLock,
  [KEY_TAB] = MKC_Tab,
  [KEY_HOME] = MKC_Home,
  [KEY_ESC] = MKC_Escape,
  [KEY_BREAK] = 0,
  [KEY_PAUSE] = MKC_Pause,
  [KEY_EQUAL] = MKC_Equal,
  [KEY_PLUS] = MKC_Equal,
  [KEY_MINUS] = MKC_Minus,
  [KEY_UNDERSCORE] = MKC_Minus,
  [KEY_BACKSLASH] = MKC_BackSlash,
  [KEY_PIPE] = MKC_BackSlash,
  [KEY_BANG] = MKC_1,
  [KEY_INSERT] = 0,
  [KEY_STAR] = MKC_8,
  [KEY_AMPERSAND] = MKC_7,
  [KEY_CARRET] = MKC_6,
  [KEY_PERCENT] = MKC_5,

  [KEY_DOT] = MKC_Period,
  [KEY_GT] = MKC_Period,

  [KEY_SEMICOLON] = MKC_SemiColon,
  [KEY_COLON] = MKC_SemiColon,

  [KEY_COMMA] = MKC_Comma,
  [KEY_LT] = MKC_Comma,

  [KEY_BACKTICK] = MKC_Grave,

  [KEY_DQUOTE] = MKC_SingleQuote,
  [KEY_APOSTROPHE] = MKC_SingleQuote,

  [KEY_QMARK] = MKC_Slash,
  [KEY_SLASH] = MKC_Slash,

  [KEY_CPAREN] = MKC_9,
  [KEY_OPAREN] = MKC_0,

  [KEY_RIGHTBRACKET] = MKC_RightBracket,
  [KEY_RIGHTBRACE] = MKC_RightBracket,

  [KEY_LEFTBRACKET] = MKC_LeftBracket,
  [KEY_LEFTBRACE] = MKC_LeftBracket,

  [KEY_RIGHT] = MKC_Right,
  [KEY_UP] = MKC_Up,
  [KEY_DOWN] = MKC_Down,
  [KEY_LEFT] = MKC_Left,
  [KEY_PAGEUP] = MKC_PageUp,
  [KEY_PAGEDOWN] = MKC_PageDown,
  [KEY_ESCAPE] = MKC_Escape,

};

static bool     kbd_map(uint8_t hid_keycode, bool pressed, uint16_t *key_out)
{
        uint8_t k = key_mapping[hid_keycode];
        if (!k)
                return false;
        if (k == 255)
                k = MKC_A; // Hack, this is zero
        k = (k << 1) | 1; // FIXME just do this in the #defines
        *key_out = k | (pressed ? 0x8000 : 0); /* Convention w.r.t. main */
        return true;
}

bool            kbd_queue_push(uint8_t hid_keycode, bool pressed)
{
        if (kbd_queue_full())
                return false;

        uint16_t v;
        if (!kbd_map(hid_keycode, pressed, &v))
                return false;

        kbd_queue[kbd_queue_prod] = v;
        kbd_queue_prod = (kbd_queue_prod + 1) & KQ_MASK;
        return true;
}
