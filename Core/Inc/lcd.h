//
// Created by Gemini on H743 Porting.
//

#ifndef LCD_H
#define LCD_H

#include "main.h"

// ---------------- 硬件引脚映射 (根据你的 H7 引脚图配置) ----------------
#define LCD_CS_PORT    GPIOA
#define LCD_CS_PIN     GPIO_PIN_4

#define LCD_DC_PORT    GPIOC
#define LCD_DC_PIN     GPIO_PIN_5

#define LCD_RST_PORT   GPIOC
#define LCD_RST_PIN    GPIO_PIN_4

#define LCD_BLK_PORT   GPIOA
#define LCD_BLK_PIN    GPIO_PIN_8

// ---------------- 快速操作宏 ----------------
#define LCD_CS_CLR     HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_SET     HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)

#define LCD_DC_CMD     HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_DATA    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

#define LCD_RST_CLR    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_SET    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

#define LCD_BLK_ON     HAL_GPIO_WritePin(LCD_BLK_PORT, LCD_BLK_PIN, GPIO_PIN_SET)
#define LCD_BLK_OFF    HAL_GPIO_WritePin(LCD_BLK_PORT, LCD_BLK_PIN, GPIO_PIN_RESET)

// ---------------- 函数声明 ----------------
void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);
void LCD_Init(void);
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_ColorFill_DMA(uint16_t *color_buf, uint32_t size);
void LCD_PushData_DMA(uint16_t *color_buf, uint32_t size);

#endif //LCD_H