#pragma once

typedef enum {
  KEY_STATE_IDLE = 0,
  KEY_STATE_PRESSED = 1,
  KEY_STATE_HOLD = 2,
  KEY_STATE_RELEASED = 3,
  KEY_STATE_LONG_HOLD = 4,
} key_state_t;

enum {
  MOD_SHIFT = 1,
  MOD_LSHIFT = 3,
  MOD_RSHIFT = 5,
  MOD_ALT = 8,
  MOD_CONTROL = 16,
};

typedef enum {
  KEY_NONE = 0,
  KEY_ERROR = -1,

  KEY_ALT = 0xA1,
  KEY_LSHIFT = 0xA2,
  KEY_RSHIFT = 0xA3,
  KEY_CONTROL = 0xA5,

  KEY_a = 'a',
  KEY_b = 'b',
  KEY_c = 'c',
  KEY_d = 'd',
  KEY_e = 'e',
  KEY_f = 'f',
  KEY_g = 'g',
  KEY_h = 'h',
  KEY_i = 'i',
  KEY_j = 'j',
  KEY_k = 'k',
  KEY_l = 'l',
  KEY_m = 'm',
  KEY_n = 'n',
  KEY_o = 'o',
  KEY_p = 'p',
  KEY_q = 'q',
  KEY_r = 'r',
  KEY_s = 's',
  KEY_t = 't',
  KEY_u = 'u',
  KEY_v = 'v',
  KEY_w = 'w',
  KEY_x = 'x',
  KEY_y = 'y',
  KEY_z = 'z',

  KEY_A = 'A',
  KEY_B = 'B',
  KEY_C = 'C',
  KEY_D = 'D',
  KEY_E = 'E',
  KEY_F = 'F',
  KEY_G = 'G',
  KEY_H = 'H',
  KEY_I = 'I',
  KEY_J = 'J',
  KEY_K = 'K',
  KEY_L = 'L',
  KEY_M = 'M',
  KEY_N = 'N',
  KEY_O = 'O',
  KEY_P = 'P',
  KEY_Q = 'Q',
  KEY_R = 'R',
  KEY_S = 'S',
  KEY_T = 'T',
  KEY_U = 'U',
  KEY_V = 'V',
  KEY_W = 'W',
  KEY_X = 'X',
  KEY_Y = 'Y',
  KEY_Z = 'Z',

  KEY_SPACE = ' ',
  KEY_TILDE = '~',
  KEY_DOLLAR = '$',

  KEY_BACKSPACE = '\b',
  KEY_ENTER = '\n',

  KEY_0 = '0',
  KEY_1 = '1',
  KEY_2 = '2',
  KEY_3 = '3',
  KEY_4 = '4',
  KEY_5 = '5',
  KEY_6 = '6',
  KEY_7 = '7',
  KEY_8 = '8',
  KEY_9 = '9',

  KEY_F1 = 0x81,
  KEY_F2 = 0x82,
  KEY_F3 = 0x83,
  KEY_F4 = 0x84,
  KEY_F5 = 0x85,
  KEY_F6 = 0x86,
  KEY_F7 = 0x87,
  KEY_F8 = 0x88,
  KEY_F9 = 0x89,
  KEY_F10 = 0x90,

  KEY_SHARP = '#',
  KEY_AT = '@',
  KEY_DELETE = 0xD4,
  KEY_END = 0xD5,
  KEY_CAPSLOCK = 0xC1,
  KEY_TAB = 0x09,
  KEY_HOME = 0xD2,
  KEY_ESC = 0xB1,
  KEY_BREAK = 0xd0,
  KEY_PAUSE = 0xd0,
  KEY_EQUAL = '=',
  KEY_PLUS = '+',
  KEY_MINUS = '-',
  KEY_UNDERSCORE = '_',
  KEY_BACKSLASH = '\\',
  KEY_PIPE = '|',
  KEY_BANG = '!',
  KEY_INSERT = 0xD1,
  KEY_STAR = '*',
  KEY_AMPERSAND = '&',
  KEY_CARRET = '^',
  KEY_PERCENT = '%',

  KEY_DOT = '.',
  KEY_GT = '>',

  KEY_SEMICOLON = ';',
  KEY_COLON = ':',

  KEY_COMMA = ',',
  KEY_LT = '<',

  KEY_BACKTICK = '`',

  KEY_DQUOTE = '"',
  KEY_APOSTROPHE = '\'',

  KEY_QMARK = '?',
  KEY_SLASH = '/',

  KEY_CPAREN = ')',
  KEY_OPAREN = '(',

  KEY_RIGHTBRACKET = ']',
  KEY_RIGHTBRACE = '}',

  KEY_LEFTBRACKET = '[',
  KEY_LEFTBRACE = '{',

  KEY_RIGHT = 0xb7,
  KEY_UP = 0xb5,
  KEY_DOWN = 0xb6,
  KEY_LEFT = 0xb4,
  KEY_PAGEUP = 0xd6,
  KEY_PAGEDOWN = 0xd7,
  KEY_ESCAPE = 0xB1,
} keycode_t;

typedef struct {
  unsigned char state;
  unsigned char modifiers;
  short code;
} input_event_t;

int keyboard_init();
input_event_t keyboard_poll();
input_event_t keyboard_wait();
char keyboard_getchar();

