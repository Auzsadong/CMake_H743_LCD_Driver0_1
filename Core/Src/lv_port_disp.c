/**
 * @file lv_port_disp_template.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"

#include <stdio.h>

#include "lcd.h"  // 包含你的底层驱动
#include "lvgl.h"
#include "ft6336u.h"
#include "spi.h"

/* 你的屏幕真实分辨率 */
#define MY_DISP_HOR_RES 320
#define MY_DISP_VER_RES 480

/* =======================================================
 * 【针对 H7 优化】将双缓冲强制分配在 D2 域的 SRAM1
 * ======================================================= */
#define DRAW_BUF_SIZE (MY_DISP_HOR_RES * 40 * 2)

#if defined(__ICCARM__)
#pragma location=0x30000000
__attribute__((aligned(32))) static uint8_t buf_1[DRAW_BUF_SIZE];
#pragma location=0x30008000 // 偏移一段距离，避免重叠
__attribute__((aligned(32))) static uint8_t buf_2[DRAW_BUF_SIZE];
#else
static uint8_t *buf_1 = (uint8_t *)0x30000000;
static uint8_t *buf_2 = (uint8_t *)0x30008000; // 偏移 32KB
#endif

/* 全局显示对象指针，留给中断回调使用 */
lv_display_t * my_disp_handle = NULL;

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    320
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    480
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */



/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init(); // 注意：确保在这个函数内部调用了你的 LCD 底层初始化代码

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    /* 2. 创建 LVGL 显示对象 */
    my_disp_handle = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    /* 3. 挂载我们在主 RAM 定义的双缓冲 */
    lv_display_set_buffers(my_disp_handle, buf_1, buf_2, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 4. 注册非阻塞的刷新回调 */
    lv_display_set_flush_cb(my_disp_handle, disp_flush);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    LCD_Init();
    FT6336U_Init();
    /*You code here*/
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/

/* 全局显示对象指针，留给中断回调使用 */




static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    printf("=> [disp_flush] area: x1:%d y1:%d x2:%d y2:%d\r\n", area->x1, area->y1, area->x2, area->y2);
    uint32_t width = lv_area_get_width(area);
    uint32_t height = lv_area_get_height(area);
    uint32_t pixel_cnt = width * height;

    printf("=> 1. LCD_SetWindow...\r\n");
    LCD_SetWindow(area->x1, area->y1, area->x2, area->y2);

    // 💣 找到真凶了！把它彻底注释掉！
    // printf("=> 2. SCB_CleanDCache...\r\n");
    // SCB_CleanDCache_by_Addr((uint32_t *)px_map, pixel_cnt * 2);

    printf("=> 3. SPI WriteCmd & CS_CLR...\r\n");
    LCD_WriteCmd(0x2C);
    LCD_DC_DATA;
    LCD_CS_CLR;

    // 🛡️ 强制解锁 HAL 状态机
    if (hspi1.State != HAL_SPI_STATE_READY) {
        printf("=> [Warning] SPI was locked! State: %x. Force Unlocking...\r\n", hspi1.State);
        __HAL_UNLOCK(&hspi1);
        hspi1.State = HAL_SPI_STATE_READY;
    }

    printf("=> 4. HAL_SPI_Transmit...\r\n");
    HAL_StatusTypeDef res = HAL_SPI_Transmit(&hspi1, (uint8_t *)px_map, pixel_cnt * 2, 1000);

    printf("=> 5. Transmit Result: %d\r\n", res);
    LCD_CS_SET;

    lv_display_flush_ready(disp);
    printf("=> 6. Flush Ready End\r\n");
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
