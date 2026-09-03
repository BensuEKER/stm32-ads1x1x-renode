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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads1x1x.h"
#include <stdio.h>
#include <string.h>
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
static ads1x1x_t adc_sensor;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static ads1x1x_status_t stm32_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
static ads1x1x_status_t stm32_i2c_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief Basit blocking UART string gonderme yardimcisi (Gorev 6:
  *        olcum sonucunun UART uzerinden gozlemlenebilir hale getirilmesi icin).
  */
static void uart_print(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
}

/**
  * @brief millivolts degerini "Voltage: <tam>.<2 basamak ondalik> mV\r\n"
  *        seklinde UART'a yazdirir. Float printf/snprintf desteginin her
  *        derleyici ayarinda acik olmayabilecegi icin ondalik kismi elle
  *        hesaplayip yaziyoruz (daha garanti bir yontem).
  */
static void uart_print_voltage(float millivolts)
{
  char buffer[48];
  int32_t whole = (int32_t)millivolts;
  int32_t frac = (int32_t)((millivolts - (float)whole) * 100.0f);
  if (frac < 0)
  {
    frac = -frac;
  }
  snprintf(buffer, sizeof(buffer), "Voltage: %ld.%02ld mV\r\n", (long)whole, (long)frac);
  uart_print(buffer);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  int16_t raw_adc_val = 0;
  float millivolts = 0.0f;
  ads1x1x_status_t status;

  /* Sensor baslatiliyor: variant = ADS1115, I2C1 uzerinden */
  status = ads1x1x_init(&adc_sensor, ADS1X1X_VARIANT_ADS1115, ADS1X1X_I2C_ADDR_GND,
                         stm32_i2c_read, stm32_i2c_write);
  if (status != ADS1X1X_OK)
  {
    Error_Handler();
  }
  uart_print("Sensor initialized\r\n");

  /* Sensor konfigurasyonu: AIN0-AIN1 diferansiyel, +-2.048V, continuous mode, 128SPS */
  status = ads1x1x_configure(&adc_sensor, ADS1X1X_MUX_AIN0_AIN1, ADS1X1X_PGA_FSR_2048MV,
                              ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);
  if (status != ADS1X1X_OK)
  {
    Error_Handler();
  }
  uart_print("Sensor configuration OK\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Sensorden ham ADC okumasi yapiliyor */
    status = ads1x1x_read_raw(&adc_sensor, &raw_adc_val);
    if (status == ADS1X1X_OK)
    {
      ads1x1x_raw_to_millivolts(&adc_sensor, raw_adc_val, ADS1X1X_PGA_FSR_2048MV, &millivolts);
      uart_print_voltage(millivolts);
    }
    else
    {
      uart_print("Sensor read error\r\n");
    }

    HAL_Delay(500);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief STM32 HAL I2C Read Wrapper (ads1x1x driver icin callback)
  * @note  STM32 HAL, I2C adresini 1 bit sola kaydirilmis (8-bit) bekler,
  *        dev_addr parametresi 7-bit cihaz adresi olarak alinir.
  */
static ads1x1x_status_t stm32_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
  /* NOT: STM32 klasik HAL kutuphanesi, tam 2 byte okunan I2C islemlerinde
   * (bizim 16-bit register'larimizin hepsi bu durumda) donanim seviyesinde
   * ozel bir POS/ACK bit sirasi kullanir (reference manual'daki "N=2 receive"
   * durumu). Renode'un STM32 I2C1 modeli bu ozel sirayi tam desteklemedigi
   * icin, gercek 2-byte okumalarda firmware sonsuz donguye takilabiliyor.
   * Cozum: 1 byte fazladan isteyip (N=3, ozel durum degil) fazlaligi atmak --
   * gercek donanimda da zararsizdir (chip fazla clock'ta veriyi tekrarlar). */
  uint8_t buffer[3];
  uint16_t read_len = (len == 2) ? 3u : len;

  if (read_len > sizeof(buffer))
  {
    return ADS1X1X_ERROR_COMM;
  }

  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev_addr << 1), reg_addr,
                                               I2C_MEMADD_SIZE_8BIT, buffer, read_len, 100);
  if (status != HAL_OK)
  {
    return ADS1X1X_ERROR_COMM;
  }

  for (uint16_t i = 0; i < len; i++)
  {
    data[i] = buffer[i];
  }
  return ADS1X1X_OK;
}

/**
  * @brief STM32 HAL I2C Write Wrapper (ads1x1x driver icin callback)
  */
static ads1x1x_status_t stm32_i2c_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dev_addr << 1), reg_addr,
                                                I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, 100);
  return (status == HAL_OK) ? ADS1X1X_OK : ADS1X1X_ERROR_COMM;
}

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