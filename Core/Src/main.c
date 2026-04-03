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
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
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

// 当 SPI DMA 传输完成时，HAL 库会自动调用这个函数
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    LCD_CS_SET;
    spi_dma_is_done = 1;
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
  /* USER CODE BEGIN 2 */
  LCD_Init();

  // 测试 100x100 窗口的刷新率
  LCD_SetWindow(0, 0, 99, 99);

  uint8_t color_toggle = 0;

  // 用于帧率统计的局部变量
  uint32_t frame_count = 0;
  uint32_t start_time = HAL_GetTick();

  char test_tx_buf[] = "Hello UART4! STM32H7 is running.\r\n";
  // 阻塞发送数据
  HAL_UART_Transmit(&huart4, (uint8_t*)test_tx_buf, strlen(test_tx_buf), HAL_MAX_DELAY);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 1. 死等上一帧 DMA 发完
    while(spi_dma_is_done == 0) {
      __NOP();
    }

    // 2. --- 核心：计算 FPS ---
    frame_count++; // 只要发完一帧，计数器加1
    // 检查是否过去了 1000 毫秒 (1秒)
    if (HAL_GetTick() - start_time >= 1000) {
      current_fps = frame_count; // 将这1秒内跑的帧数存入全局变量
      frame_count = 0;           // 清零计数器，准备下1秒
      start_time = HAL_GetTick();// 重置起始时间

      // 让 LED 跟着心跳每秒闪烁一次，证明单片机没死机而且在疯狂工作
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

      printf("TFT DMA Refresh Rate: %lu FPS\r\n", current_fps);
    }

    // 3. 准备下一帧的颜色数据 (包含大小端转换)
    uint16_t current_color = (color_toggle == 0) ? 0xF800 : 0x001F;
    color_toggle = !color_toggle;
    current_color = (current_color >> 8) | (current_color << 8);

    // CPU 疯狂填满显存
    for(uint32_t i = 0; i < 100 * 100; i++) {
      test_color_buf[i] = current_color;
    }

    // 4. 启动 DMA (注意这里没有 HAL_Delay 了！)
    spi_dma_is_done = 0;
    LCD_ColorFill_DMA(test_color_buf, 100 * 100);
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
