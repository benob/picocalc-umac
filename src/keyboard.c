#include <stdio.h>

#include <pico/stdio.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>

#include "keyboard.h"

#define KBD_MOD    i2c1
#define KBD_SDA    6
#define KBD_SCL    7
#define KBD_SPEED  20000 // if dual i2c, then the speed of keyboard i2c should be 10khz
#define KBD_ADDR   0x1F

// Commands defined by the keyboard driver
enum {
  REG_ID_VER = 0x01,     // fw version
  REG_ID_CFG = 0x02,     // config
  REG_ID_INT = 0x03,     // interrupt status
  REG_ID_KEY = 0x04,     // key status
  REG_ID_BKL = 0x05,     // backlight
  REG_ID_DEB = 0x06,     // debounce cfg
  REG_ID_FRQ = 0x07,     // poll freq cfg
  REG_ID_RST = 0x08,     // reset
  REG_ID_FIF = 0x09,     // fifo
  REG_ID_BK2 = 0x0A,     //keyboard backlight
  REG_ID_BAT = 0x0b,     // battery
  REG_ID_C64_MTX = 0x0c, // read c64 matrix
  REG_ID_C64_JS = 0x0d,  // joystick io bits
};

static int keyboard_modifiers;

static int i2c_kbd_write(unsigned char* data, int size) {
  int retval = i2c_write_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, 500000);
  if (retval == PICO_ERROR_GENERIC || retval == PICO_ERROR_TIMEOUT) {
    printf("i2c_kbd_write: i2c write error\n");
    return 0;
  }
  return 1;
}

static int i2c_kbd_read(unsigned char* data, int size) {
  int retval = i2c_read_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, 500000);
  if (retval == PICO_ERROR_GENERIC || retval == PICO_ERROR_TIMEOUT) {
    printf("i2c_kbd_read: i2c read error\n");
    return 0;
  }
  return 1;
}

static int i2c_kbd_command(unsigned char command) {
  return i2c_kbd_write(&command, 1);
}

static int i2c_kbd_queue_size() {
  if (!i2c_kbd_command(REG_ID_KEY)) return 0; // Read queue size 
  unsigned short result = 0;
  if (!i2c_kbd_read((unsigned char*)&result, 2)) return 0;
  return result & 0x1f; // bits beyond that mean something different
}

static unsigned short i2c_kbd_read_key() {
  if (!i2c_kbd_command(REG_ID_FIF)) return 0;
  unsigned short result = 0;
  if (!i2c_kbd_read((unsigned char*)&result, 2)) return KEY_NONE;
  return result;
}

static void update_modifiers(unsigned short value) {
  switch (value) {
    case (KEY_CONTROL << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_CONTROL; break;
    case (KEY_CONTROL << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_CONTROL; break;
    case (KEY_ALT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_ALT; break;
    case (KEY_ALT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_ALT; break;
    case (KEY_LSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_LSHIFT; break;
    case (KEY_LSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_LSHIFT; break;
    case (KEY_RSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_RSHIFT; break;
    case (KEY_RSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_RSHIFT; break;
  }
}

#include "pico/bootrom.h"
#include "hardware/watchdog.h"

static void keyboard_check_special_keys(unsigned short value) {
  if ((value & 0xff) == KEY_STATE_RELEASED && keyboard_modifiers == (MOD_CONTROL|MOD_ALT)) {
    if ((value >> 8) == KEY_F1) {
      printf("rebooting to usb boot\n");
      reset_usb_boot(0, 0);
    } else if ((value >> 8) == KEY_DELETE) {
      printf("rebooting via watchdog\n");
      watchdog_reboot(0, 0, 0);
      watchdog_enable(0, 1);
    }
  }
}

input_event_t keyboard_poll() {
  unsigned short value = i2c_kbd_read_key();
  update_modifiers(value);
  keyboard_check_special_keys(value);
  //if (value != 0 && (value >> 8) != KEY_ALT && (value >> 8) != KEY_CONTROL) printf("key = %d (%02x) / state = %d / modifiers = %02x\n", value >> 8, value >> 8, value & 0xff, keyboard_modifiers);
  return (input_event_t) {value & 0xff, keyboard_modifiers, value >> 8};
}

input_event_t keyboard_wait() {
  input_event_t event;
  do { 
    event = keyboard_poll();
    if (event.code == 0) sleep_ms(1);
  } while (event.code == 0);
  return event;
}

char keyboard_getchar() {
  return keyboard_wait().code;
}

int keyboard_init() {
  i2c_init(KBD_MOD, KBD_SPEED);
  gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
  gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(KBD_SCL);
  gpio_pull_up(KBD_SDA);
  keyboard_modifiers = 0;
  while (i2c_kbd_read_key() != 0); // Drain queue
}

