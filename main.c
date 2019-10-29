#include "main.h"
#include <stdlib.h>

#define CHAR_BUFF_SIZE 12*sizeof(char)
#define DC_CALIBRATION_SIGNAL 1241 // equivalent of 1 volt in the 0-4095 3.3V range
#define ADC_BUFFER_SIZE 100

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define EXTRACT_ADC1_DATA(DATA) ((DATA) & 0x0000FFFF)
#define EXTRACT_ADC2_DATA(DATA) ((DATA) >> 16)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch2;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
__IO uint32_t aADCRawValues[ADC_BUFFER_SIZE]; // holds the converted ADC1 values
uint16_t aADC1ConvertedValues[ADC_BUFFER_SIZE];
uint16_t aADC2ConvertedValues[ADC_BUFFER_SIZE];
__IO uint8_t adc_conversion_has_finished = 0;

const uint16_t one_volt[16] = {
		1241, 1241, 1241, 1241, 1241, 1241, 1241, 1241,
		1241, 1241, 1241, 1241, 1241, 1241, 1241, 1241
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

// printing utilities
static void _print(char*); // print a char[] string to SWO
static void _print_int(uint32_t); // convert uint32_t to char[] & print to SWO
static char* _int_to_char(uint32_t, char*);

void runTest(void);
/* USER CODE END PFP */

/******************************* ADC CONVERSION ************************************/

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	if (HAL_ADCEx_MultiModeStop_DMA(&hadc1) != HAL_OK) {
		_print("can't stop ADC\n");
	}
	if (HAL_ADC_Stop_IT(&hadc2) != HAL_OK) {
		_print("can't stop slave ADC\n");
	}
	for(uint16_t i=0; i < ADC_BUFFER_SIZE; i++) {
		aADC1ConvertedValues[i] = EXTRACT_ADC1_DATA(aADCRawValues[i]);
		aADC2ConvertedValues[i] = EXTRACT_ADC2_DATA(aADCRawValues[i]);
	}
	adc_conversion_has_finished = 1;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc) {
	_print("error ADC\n");
}

/******************************* PRINTING & UTILITIES ************************************/
static void _delay_ms(uint16_t delay) {
    uint32_t cycles = (delay * 64000)/5;  // 64 Mhz, CMP+BEQ+NOP+ADDS+B
    for(uint32_t i=0; i < cycles; i++) {
        __asm__("nop");
    }
}

// Function for printing a string through the SWO interface. String may be 256 chars max.
static void _print(char data[]) {
	for(uint8_t i = 0; data[i] != '\0'; i++) {
		ITM_SendChar(data[i]);
	}
}

static void _print_int(uint32_t x) {
	char *p = malloc(CHAR_BUFF_SIZE);
	for(uint8_t i=0; i < CHAR_BUFF_SIZE; i++) {
		p[i] = (char)0;
	}
	_print(_int_to_char(x, p));
	free(p);
}

static char * _int_to_char(uint32_t x, char *p) {
	char *s = p + CHAR_BUFF_SIZE;
	*--s = 0; // null terminate
	if (!x) *--s = '0';
	for (; x; x/=10) *--s = '0'+x%10;
	return s;
}

void runTest(void) {
	  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) _print("can't calibrate ADC1");
	  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) _print("can't calibrate ADC2");

	  HAL_TIM_Base_Start(&htim6);
	  HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
	  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t*)one_volt, 16, DAC_ALIGN_12B_R);

	  uint32_t adc1, adc2;
	  while (1) {
		adc1 = 0;
		adc2 = 0;
		_delay_ms(500);

		if (HAL_ADC_Start(&hadc2) != HAL_OK) {
			_print("can't start slave ADC\n");
		}
		// enable ADC master
		if (HAL_ADCEx_MultiModeStart_DMA(
					    &hadc1,
						(uint32_t *)aADCRawValues,
						ADC_BUFFER_SIZE
	    	    		) != HAL_OK) {
			_print("can't start master ADC\n");
	    	// TODO: handle error
	    }
		while(!adc_conversion_has_finished) {}
		//_print("total val = "); _print_int(val); _print(", val = ");

		for(uint16_t i=0; i < ADC_BUFFER_SIZE; i++) {
			adc1 += aADC1ConvertedValues[i];
			adc2 += aADC2ConvertedValues[i];
		}
		adc1 /= ADC_BUFFER_SIZE;
		adc2 /= ADC_BUFFER_SIZE;
		_print_int(adc1); _print(",");
		_print_int(adc2); _print("\n");
		//_print_int(val); _print("\n");
		adc_conversion_has_finished = 0;
	  }

	  // we should never reach here
	  HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
}

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_DAC1_Init();
  MX_TIM6_Init();
  runTest();
  while (1) {}
}

void SystemClock_Config(void){
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_ADC1_Init(void) {
  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    Error_Handler();
  }
  multimode.Mode = ADC_DUALMODE_REGSIMULT;
  multimode.DMAAccessMode = ADC_DMAACCESSMODE_12_10_BITS;
  multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
    Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_ADC2_Init(void){
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc2) != HAL_OK) {
    Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK){
    Error_Handler();
  }

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void) {
  DAC_ChannelConfTypeDef sConfig = {0};
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK){
    Error_Handler();
  }
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputSwitch = DAC_OUTPUTSWITCH_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK){
    Error_Handler();
  }
}

static void MX_TIM6_Init(void) {
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 200;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

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
void assert_failed(char *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
