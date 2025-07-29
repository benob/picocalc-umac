#pragma once

void video_init(uint32_t *framebuffer);
void video_update();
void fb_fill_rect(int x, int y, int width, int height, uint8_t color);
void fb_draw_char(int x, int y, uint8_t color, char c);
void fb_draw_text(int x, int y, uint8_t color, const char* text);
void fb_printf(int x, int y, uint8_t color, const char* format, ...);
