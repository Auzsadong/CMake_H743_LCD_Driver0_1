#ifndef __FT6336U_H
#define __FT6336U_H

#include "main.h"

/* I2C 设备地址 (7位地址 0x38, HAL库需左移一位) */
#define FT6336U_ADDR_WRITE    0x70
#define FT6336U_ADDR_READ     0x71

/* 核心寄存器地址 */
#define FT_REG_MODE_SWITCH    0x00    // 模式切换寄存器
#define FT_REG_TD_STATUS      0x02    // 触摸点数寄存器
#define FT_REG_P1_XH          0x03    // 触点1坐标高位
#define FT_REG_P1_XL          0x04    // 触点1坐标低位
#define FT_REG_P1_YH          0x05    // 触点1坐标高位
#define FT_REG_P1_YL          0x06    // 触点1坐标低位
#define FT_REG_CHIPID         0xA8    // 芯片 ID 寄存器

/* 预期的芯片 ID */
#define FT6336U_ID_VALUE      0x11

/* 触摸信息结构体 */
typedef struct {
    uint8_t  is_pressed;  // 1: 按下, 0: 松开
    uint16_t x;           // X 坐标 (0-320)
    uint16_t y;           // Y 坐标 (0-480)
} FT6336U_Touch_t;

/* 外部调用接口 */
uint8_t FT6336U_Init(void);
void FT6336U_Get_Touch(FT6336U_Touch_t *touch);

#endif /* __FT6336U_H */