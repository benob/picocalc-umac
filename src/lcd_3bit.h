#pragma once

#include "types.h"

#define WIDTH 320
#define HEIGHT 320
#define MEM_HEIGHT 480 

#define RED(a)      (((a) & 0x4) >> 2)
#define GREEN(a)    (((a) & 0x2) >> 1)
#define BLUE(a)     ((a) & 0x1)

#define RGB(r,g,b) ((u8)(((r != 0) << 2) | ((g != 0) << 1) | (b != 0)))

void lcd_draw(u8* pixels, int x, int y, int width, int height);
void lcd_fill(u8 color, int x, int y, int width, int height);
int lcd_clear();
void lcd_draw_char(int x, int y, u8 fg, u8 bg, char c);
void lcd_draw_text(int x, int y, u8 fg, u8 bg, const char* text);
void lcd_printf(int x, int y, u8 fg, u8 bg, const char* format, ...);

typedef struct {
  u8* glyphs __attribute__((aligned(4)));
  int glyph_width;
  int glyph_height;
} font_t;

extern font_t font;

void lcd_init();
void lcd_on();
void lcd_off();
void lcd_blank();
void lcd_unblank();
void lcd_setup_scrolling(int top_fixed_lines, int bottom_fixed_lines);
void lcd_scroll(int lines);

