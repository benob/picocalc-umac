#pragma once

#include "types.h"

#define WIDTH 320
#define HEIGHT 320
#define MEM_HEIGHT 480 

#define RED(a)      ((((a) & 0xf800) >> 11) << 3)
#define GREEN(a)    ((((a) & 0x07e0) >> 5) << 2)
#define BLUE(a)     (((a) & 0x001f) << 3)

#define RGB(r,g,b) ((u16)(((r) >> 3) << 11 | ((g) >> 2) << 5 | (b >> 3)))

void lcd_draw(u16* pixels, int x, int y, int width, int height);
void lcd_fill(u16 color, int x, int y, int width, int height);
int lcd_clear();
void lcd_draw_char(int x, int y, u16 fg, u16 bg, char c);
void lcd_draw_text(int x, int y, u16 fg, u16 bg, const char* text);
void lcd_printf(int x, int y, u16 fg, u16 bg, const char* format, ...);

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

