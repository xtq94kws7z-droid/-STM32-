/**
 * oled.h - SSD1306 OLED 128x64 I2C 驱动
 * 适用: STM32F103C8T6 + HAL库
 * I2C 地址: 0x3C
 */

#ifndef __OLED_H
#define __OLED_H

#include "main.h"

#define OLED_ADDR       0x3C    /* 7位I2C地址 */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

/* 帧缓冲区 (公开给 main.c 使用) */
extern uint8_t OLED_Buffer[OLED_WIDTH * OLED_HEIGHT / 8];

/* API */
void OLED_Init(I2C_HandleTypeDef *hi2c);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(uint8_t x, uint8_t y, char ch);
void OLED_Print(uint8_t x, uint8_t y, const char *str);
void OLED_DrawChinese(uint8_t x, uint8_t y, uint8_t index);

#endif /* __OLED_H */
