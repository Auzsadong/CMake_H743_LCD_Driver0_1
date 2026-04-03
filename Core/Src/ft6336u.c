#include "ft6336u.h"
#include "i2c.h"
#include <stdio.h>

/**
 * @brief FT6336U 硬件复位与初始化
 * @return 0: 成功, 1: 失败 (ID不匹配或通信异常)
 */
uint8_t FT6336U_Init(void) {
    uint8_t chip_id = 0;

    /* 1. 硬件复位时序 (使用新的 PC0 引脚) */
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20); 
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100); // 等待芯片内部固件加载完成

    /* 2. 读取芯片 ID 验证通信 */
    if (HAL_I2C_Mem_Read(&hi2c1, FT6336U_ADDR_READ, FT_REG_CHIPID, I2C_MEMADD_SIZE_8BIT, &chip_id, 1, 100) != HAL_OK) {
        printf("[FT6336U] I2C 通信错误!\r\n");
        return 1;
    }

    if (chip_id != FT6336U_ID_VALUE) {
        printf("[FT6336U] 芯片 ID 不匹配! 读到: 0x%02X, 预期: 0x51\r\n", chip_id);
        return 1;
    }

    /* 3. 配置为正常操作模式 (写 0x00 到模式寄存器) */
    uint8_t mode = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, FT6336U_ADDR_WRITE, FT_REG_MODE_SWITCH, I2C_MEMADD_SIZE_8BIT, &mode, 1, 100);

    printf("[FT6336U] 初始化成功, ID: 0x%02X\r\n", chip_id);
    return 0;
}

/**
 * @brief 读取当前触摸坐标
 * @param touch 指向存储结果的结构体指针
 */
void FT6336U_Get_Touch(FT6336U_Touch_t *touch) {
    uint8_t data_buf[4];
    uint8_t touch_num = 0;

    HAL_I2C_Mem_Read(&hi2c1, FT6336U_ADDR_READ, FT_REG_TD_STATUS, I2C_MEMADD_SIZE_8BIT, &touch_num, 1, 10);
    touch_num &= 0x0F;

    if (touch_num > 0) {
        if (HAL_I2C_Mem_Read(&hi2c1, FT6336U_ADDR_READ, FT_REG_P1_XH, I2C_MEMADD_SIZE_8BIT, data_buf, 4, 50) == HAL_OK) {

            // 1. 获取原始坐标
            uint16_t raw_x = ((uint16_t)(data_buf[0] & 0x0F) << 8) | data_buf[1];
            uint16_t raw_y = ((uint16_t)(data_buf[2] & 0x0F) << 8) | data_buf[3];

            // 2. 坐标镜像翻转
            // 原理：新坐标 = 最大值 - 原始值
            // 注意：320x480 的最大索引是 319 和 479 (或根据你实测的最大值调整)
            touch->x = 319 - raw_x;
            touch->y = 479 - raw_y;

            touch->is_pressed = 1;
        }
    } else {
        touch->is_pressed = 0;
    }
}