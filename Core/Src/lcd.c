//
// Created by Gemini on H743 Porting.
//

#include "lcd.h"
#include "spi.h"

// 引入 CubeMX 生成的 SPI1 句柄
extern SPI_HandleTypeDef hspi1;

/* * 1. 发送指令函数
 */
void LCD_WriteCmd(uint8_t cmd) {
    LCD_DC_CMD;
    LCD_CS_CLR;
    // 阻塞发送 1 个字节指令
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_SET;
}

/* * 2. 发送数据函数
 */
void LCD_WriteData(uint8_t data) {
    LCD_DC_DATA;
    LCD_CS_CLR;
    // 阻塞发送 1 个字节数据
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    LCD_CS_SET;
}

/* * 3. 屏幕初始化函数 (复用你的原厂序列)
 */
void LCD_Init(void) {
    // 1. 硬件复位
    LCD_RST_CLR;
    HAL_Delay(50);
    LCD_RST_SET;
    HAL_Delay(120); // 必须等待复位完成

    // 2. 原厂初始化序列开始
    LCD_WriteCmd(0x11); // Sleep Out
    HAL_Delay(120);

    LCD_WriteCmd(0xf0); LCD_WriteData(0xc3);
    LCD_WriteCmd(0xf0); LCD_WriteData(0x96);
    LCD_WriteCmd(0x36); LCD_WriteData(0x48); // 方向控制
    LCD_WriteCmd(0x3A); LCD_WriteData(0x55); // 16位像素格式
    LCD_WriteCmd(0xB4); LCD_WriteData(0x01);
    LCD_WriteCmd(0xB7); LCD_WriteData(0xC6);

    LCD_WriteCmd(0xe8);
    LCD_WriteData(0x40); LCD_WriteData(0x8a); LCD_WriteData(0x00);
    LCD_WriteData(0x00); LCD_WriteData(0x29); LCD_WriteData(0x19);
    LCD_WriteData(0xa5); LCD_WriteData(0x33);

    LCD_WriteCmd(0xc1); LCD_WriteData(0x06);
    LCD_WriteCmd(0xc2); LCD_WriteData(0xa7);
    LCD_WriteCmd(0xc5); LCD_WriteData(0x18);

    // 伽马校正
    LCD_WriteCmd(0xe0);
    LCD_WriteData(0xf0); LCD_WriteData(0x09); LCD_WriteData(0x0b);
    LCD_WriteData(0x06); LCD_WriteData(0x04); LCD_WriteData(0x15);
    LCD_WriteData(0x2f); LCD_WriteData(0x54); LCD_WriteData(0x42);
    LCD_WriteData(0x3c); LCD_WriteData(0x17); LCD_WriteData(0x14);
    LCD_WriteData(0x18); LCD_WriteData(0x1b);

    LCD_WriteCmd(0xe1);
    LCD_WriteData(0xf0); LCD_WriteData(0x09); LCD_WriteData(0x0b);
    LCD_WriteData(0x06); LCD_WriteData(0x04); LCD_WriteData(0x03);
    LCD_WriteData(0x2d); LCD_WriteData(0x43); LCD_WriteData(0x42);
    LCD_WriteData(0x3b); LCD_WriteData(0x16); LCD_WriteData(0x14);
    LCD_WriteData(0x17); LCD_WriteData(0x1b);

    LCD_WriteCmd(0xf0); LCD_WriteData(0x3c);
    LCD_WriteCmd(0xf0); LCD_WriteData(0x69);
    HAL_Delay(120);

    LCD_WriteCmd(0x29); // Display on

    // 3. 点亮背光
    LCD_BLK_ON;
}

/* * 4. 设置刷新窗口函数
 */
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t data[4];

    // 1. 设置列地址 (0x2A)
    LCD_DC_CMD;
    LCD_CS_CLR;
    uint8_t cmd_2a = 0x2A;
    HAL_SPI_Transmit(&hspi1, &cmd_2a, 1, HAL_MAX_DELAY); // 发送指令
    LCD_DC_DATA;
    data[0] = x1 >> 8; data[1] = x1 & 0xFF;
    data[2] = x2 >> 8; data[3] = x2 & 0xFF;
    HAL_SPI_Transmit(&hspi1, data, 4, HAL_MAX_DELAY);    // 连续发送4个参数
    LCD_CS_SET; // 全部发完再拉高 CS

    // 2. 设置行地址 (0x2B)
    LCD_DC_CMD;
    LCD_CS_CLR;
    uint8_t cmd_2b = 0x2B;
    HAL_SPI_Transmit(&hspi1, &cmd_2b, 1, HAL_MAX_DELAY);
    LCD_DC_DATA;
    data[0] = y1 >> 8; data[1] = y1 & 0xFF;
    data[2] = y2 >> 8; data[3] = y2 & 0xFF;
    HAL_SPI_Transmit(&hspi1, data, 4, HAL_MAX_DELAY);
    LCD_CS_SET;
}

/* * 5. DMA 颜色填充函数 (H7 特化版)
 */
void LCD_ColorFill_DMA(uint16_t *color_buf, uint32_t size) {
    // 1. 清理 Cache (AXI SRAM 在 D1 域，必须清理 Cache)
    SCB_CleanDCache_by_Addr((uint32_t *)color_buf, size * 2);

    LCD_WriteCmd(0x2C);
    LCD_DC_DATA;
    LCD_CS_CLR;

    // 2. 启动 DMA 并检查返回值
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)color_buf, size * 2);

    // 如果 status 不是 HAL_OK (0)，说明由于某种原因(比如未就绪、参数错)根本没发车
    if (status != HAL_OK)
    {
        // 在 CLion 里可以在这行打个断点，如果停在这里，说明 HAL 库状态锁死了
        __NOP();
    }
}