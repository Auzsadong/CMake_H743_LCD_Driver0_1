/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "quadspi.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ft6336u.h"
#include "lcd.h"
#include "lvgl.h"            // 新增
#include "lv_port_disp.h"    // 新增
#include "lv_port_indev.h"   // 新增
#include "ui.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint16_t *test_color_buf = (uint16_t *)0x30000000;
// 这是一个标志位，加上 volatile 防止被编译器过度优化
volatile uint8_t spi_dma_is_done = 1;

volatile uint32_t current_fps = 0; // 存放最终计算出的实时帧率
extern lv_display_t * my_disp_handle;

// 👇 加上这一句，告诉 main.c 你的大壁纸在外部编译单元里
LV_IMG_DECLARE(my_wallpaper);

// 当 SPI DMA 传输完成时，HAL 库会自动调用这个函数
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    LCD_CS_SET;
    spi_dma_is_done = 1;
    if(my_disp_handle != NULL) {
      lv_display_flush_ready(my_disp_handle);
    }
  }
}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    // 如果触发了错误，片选拉高，强行把标志位置1，防止程序死锁
    LCD_CS_SET;
    spi_dma_is_done = 1;
  }
}
// 重写 GCC 的底层系统调用，将 printf 的数据流引向 UART4
int _write(int file, char *ptr, int len) {
  // 使用阻塞模式发送数据
  HAL_UART_Transmit(&huart4, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

// 包含 MPU 和 QSPI 需要的头文件环境
extern QSPI_HandleTypeDef hqspi;

// 开启 QSPI 内存映射模式 (终极防卡死版)
void QSPI_Enable_MemoryMappedMode(void) {
    // ====================================================================
    // 0. 填坑：强制上拉 IO2(WP) 和 IO3(HOLD) 引脚，防止浮空导致 Flash 锁死
    // ====================================================================
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 【绝对核心：强制开启内部上拉】
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;

    // 重新初始化 PD11, PD12, PD13(HOLD)
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    // 重新初始化 PE2(WP)
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // ====================================================================
    // 1. 软件复位 W25Q64，唤醒并清除所有异常状态
    // ====================================================================
    QSPI_CommandTypeDef sCommand = {0};
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;

    sCommand.Instruction = 0x66; // 发送 Reset Enable
    HAL_QSPI_Command(&hqspi, &sCommand, HAL_MAX_DELAY);

    sCommand.Instruction = 0x99; // 发送 Reset
    HAL_QSPI_Command(&hqspi, &sCommand, HAL_MAX_DELAY);
    HAL_Delay(50); // 等待复位完成

    // ====================================================================
    // 2. MPU 配置：设为 Normal Memory (Non-cacheable)，兼容 AXI 总线
    // ====================================================================
    MPU_Region_InitTypeDef MPU_InitStruct = {0};
    HAL_MPU_Disable();
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x90000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
    MPU_InitStruct.SubRegionDisable = 0x0;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1; // 【关键】设为普通内存
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE; // 依然不使用 Cache 保证数据一致性
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);
  // ====================================================================
  // MPU 区域 2 配置：AXI SRAM (0x24000000) 专供 LVGL 使用，必须关闭 Cache
  // ====================================================================
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2; // 使用 2 号区域
  MPU_InitStruct.BaseAddress = 0x24000000;    // AXI SRAM 起始地址
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;   // 【关键！关掉 Cache】
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    // ====================================================================
    // 3. 配置指令：进入内存映射 (使用 0x0B Fast Read)
    // ====================================================================
    QSPI_MemoryMappedTypeDef sMemMappedCfg = {0};

    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = 0x0B;                    // Fast Read (支持最高 133MHz)
    sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
    sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 8;                       // 0x0B 指令强制需要 8 个空跑周期
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    sMemMappedCfg.TimeOutPeriod     = 0;

    if (HAL_QSPI_MemoryMapped(&hqspi, &sCommand, &sMemMappedCfg) != HAL_OK) {
        printf("QSPI Memory Mapped Mode Failed!\r\n");
    } else {
        printf("QSPI Memory Mapped Mode Success! Mapped to 0x90000000\r\n");
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_UART4_Init();
  MX_QUADSPI_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  // 1. 开启 QSPI 内存映射
  QSPI_Enable_MemoryMappedMode();

  // 2. 暴力读取测试！直接去读 0x90000000 的数据
  uint8_t *flash_ptr = (uint8_t *)0x90000000;

  printf("\r\n--- W25Q64 Read Test ---\r\n");
  printf("Read Flash [0]: 0x%02X\r\n", flash_ptr[0]);
  printf("Read Flash [1]: 0x%02X\r\n", flash_ptr[1]);
  printf("Read Flash [2]: 0x%02X\r\n", flash_ptr[2]);
  printf("Read Flash [3]: 0x%02X\r\n", flash_ptr[3]);
  printf("------------------------\r\n");
  
  if (FT6336U_Init() == 0) {
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); // 成功则点亮 LED1
  }

  printf("=> 1. LVGL Core Init...\r\n");
  lv_init();

  printf("=> 2. Display Port Init...\r\n");
  lv_port_disp_init();

  printf("=> 3. Input Port Init...\r\n");
  lv_port_indev_init();

  printf("=> 4. UI Init...\r\n");

  ui_init();

  // 👇 开始注入我们的动态壁纸
  printf("=> 5. Injecting Dynamic Wallpaper from QSPI...\r\n");
  if (ui_uiBgPanel == NULL) {
    printf("!!! ERROR: ui_uiBgPanel is NULL!\r\n");
  } else {
    // 【防坑 1】强制把 SquareLine 默认的白色背景变成完全透明！
    lv_obj_set_style_bg_opa(ui_uiBgPanel, 0, LV_PART_MAIN);

    // 尝试创建 GIF 对象
    lv_obj_t * dynamic_bg = lv_gif_create(ui_uiBgPanel);

    if (dynamic_bg == NULL) {
      // 【防坑 2】如果走到这里，100% 是 LV_MEM_SIZE 还不够大
      printf("!!! FATAL: GIF Create Failed! (Out of RAM?)\r\n");
    } else {
      // 成功创建，塞入图片数据
      lv_gif_set_src(dynamic_bg, &my_wallpaper);
      lv_obj_align(dynamic_bg, LV_ALIGN_CENTER, 0, 0);

      // 👇 加上这一句：强行把 GIF 移动到 UI 树的最顶层，确保没有人能遮挡它
      // 测试成功后，如果你需要按钮在 GIF 上面，再把这句删掉
      lv_obj_move_foreground(dynamic_bg);

      printf("GIF Inject Success! Address: %p\r\n", dynamic_bg);
    }
  }

  printf("LVGL Test Started...\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


    // 1. 处理 LVGL 核心任务
    lv_timer_handler();

    // 2. 硬件延时 5 毫秒
    HAL_Delay(5);

    // 3. 【极度关键】给 LVGL 打心脏起搏器，告诉它过去了 5 毫秒！
    // 如果没有这句话，GIF 永远静止，啥也画不出来！
    lv_tick_inc(5);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
