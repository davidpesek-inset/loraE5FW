/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
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
#include "app_lorawan.h"
#include "gpio.h"
#include "iwdg.h"
#include "eeprom_emul.h"
#include "eeprom_wrapper.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sys_conf.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lw_core.h"
#include "lw_config.h"
#include "lw_link.h"
#include "lw_options.h"
#include "dev_system.h"
#include "lora_app.h"
#include "LoRaMac.h"
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
uint8_t nvm_dirty = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void init_pre_join();
static void init_post_join();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	__HAL_RCC_CRC_CLK_ENABLE();
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

  HAL_FLASH_Unlock();
  if (EE_Init(EE_FORCED_ERASE) != EE_OK)
  {
    Error_Handler();
  }
  eeprom_vars_init();
  HAL_FLASH_Lock();

  MX_LoRaWAN_Init();	/* UART, trace, RTC, UTIL_TIMER */
  if (DEBUGGING == 1){
	  dev_log_set(VLEVEL_H);
  }
  if (eeprom_vars.wdt == 1U)
    {
      MX_IWDG_Init();                  // potřebuje UTIL_TIMER a RTC
    }
  init_pre_join();
  LoRaWAN_StartAbp();
  init_post_join();

  while (1)
  {
    MX_LoRaWAN_Process();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  /* HSI16 is the USART1 kernel clock (needed for wake-up from Stop mode on start bit) */
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void init_pre_join(){
	uint32_t init_phase = 0;
	if (EE_ReadVariable32bits(EE_VA_INIT_PHASE, &init_phase) != EE_OK){
		return;
	}
	if (init_phase == 2){
		DEBUG_PRINTF("Modem already initialized, skipping configuration\r\n");
		return;
	}
	if (init_phase != 1){
		DEBUG_PRINTF("Initialization error - wrong state\r\n");
		return;
	}

	if (lw_region_set(LORAMAC_REGION_EU868) != LW_OK){
		return;
	}

	if (lw_appskey_set(ram_vars.appskey) != LW_OK){
			return;
		}

	if (lw_nwkskey_set(ram_vars.nwkskey) != LW_OK){
		return;
	}

	if (lw_devaddr_set(ram_vars.devaddr) != LW_OK){
		return;
	}

}

static void init_post_join(){
	uint32_t init_phase = 0;
	uint8_t num_ch = 0;
	if (EE_ReadVariable32bits(EE_VA_INIT_PHASE, &init_phase) != EE_OK){
		return;
	}
	// wait for ABP join finish
	while (LoRaMacIsBusy()) { MX_LoRaWAN_Process(); }
	// all config, that saves to NVM - set once on first boot
	if (init_phase == 1){
		num_ch = sizeof(ram_vars.ch_data)/sizeof(ram_vars.ch_data[0]);
		for (size_t i = 0; i < num_ch; i++){
			if (lw_ch_set(i, ram_vars.ch_data[i].drmin, ram_vars.ch_data[i].drmax, ram_vars.ch_data[i].freq) != LW_OK){
				return;
			}
		}
		if (lw_class_set(ram_vars.class) != LW_OK){
			return;
		}
		if (lw_adr_set(ram_vars.adr) != LW_OK){
			return;
		}
		if (lw_power_set(ram_vars.power, true) != LW_OK){
			return;
		}
		if (lw_rx1Delay_set(ram_vars.rx1Delay) != LW_OK){
			return;
		}
		if (lw_rx2Delay_set(ram_vars.rx2Delay) != LW_OK){
			return;
		}
		if (lw_rept_set(ram_vars.rept) != LW_OK){
			return;
		}
		if (lw_nvm_store(0U) != LW_OK){
			return;
		}
		eeprom_set_init_phase(2);
	}

	if (lw_dr_set(eeprom_vars.dr) != LW_OK){
		return;
	}
	if (lw_dc_set(eeprom_vars.dc) != LW_OK){
		return;
	}
	if (lw_net_set(eeprom_vars.net) != LW_OK){
		return;
	}
	//if (lw_ldro_set(eeprom_vars.ldro) != LW_OK){
	//		return;
	//}
	if (lw_thld_set(eeprom_vars.thld_dbm) != LW_OK){
		return;
	}
	//if (lw_dumrx_set(eeprom_vars.dumrx) != LW_OK){
	//		return;
	//}
	if (lw_ct_set(eeprom_vars.ct) != LW_OK){
		return;
	}

	if (lw_port_set(eeprom_vars.port) != LW_OK){
		return;
	}
	if (lw_retry_set(eeprom_vars.retry) != LW_OK){
		return;
	}
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

#ifdef  USE_FULL_ASSERT
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
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
