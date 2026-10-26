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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* ==========================自定义用户头文件================================= */
#include "LED.h"
#include "BEEP.h"
#include "KEY.h"
#include "SERVO.h"
#include "ADC_Read.h"
#include "HC05.h"
#include "ESP01S.h"
#include "BEMFA.h"
#include "CloudCmd.h"
#include "BtConfig.h"
#include "at24c02.h"
#include "WifiStorage.h"
#include "Oled.h"
#include "bmp.h"
#include "oledfonts.h"
#include "stm32f4xx_it.h"
#include "UI_Preview.h"
#include "UI_Terminal.h"

/* ==========================系统头文件======================================= */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/**
 * @brief Wi-Fi 配置队列项
 * @note  CubeMX 中 WifiCfgQueue 的单个消息大小配置为 64 字节。
 *        BtConfig_Data 本身不足 64 字节，这里补齐到 64 字节，
 *        方便队列按固定长度复制。
 */
typedef struct
{
  BtConfig_Data data;
  uint8_t reserved[6];
} WifiCfgQueueItem;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ADC 数值变化超过该阈值才打印，避免串口被刷屏。 */
#define ADC_PRINT_CHANGE_THRESHOLD 100U

/* 网络事件标志位：HeartBeatTask 每 30 秒置位一次。 */
#define NET_EVT_HEARTBEAT (1U << 0)

/* 按键事件标志位：由 EXTI 中断置位，KeyHandleTask 处理。 */
#define KEY_EVT_KEY0 (1U << 0)
#define KEY_EVT_KEY1 (1U << 1)
#define KEY_EVT_KEY2 (1U << 2)
#define KEY_EVT_KEY3 (1U << 3)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for NetWorkTask */
osThreadId_t NetWorkTaskHandle;
const osThreadAttr_t NetWorkTask_attributes = {
    .name = "NetWorkTask",
    .stack_size = 512 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for HeartBeatTask */
osThreadId_t HeartBeatTaskHandle;
const osThreadAttr_t HeartBeatTask_attributes = {
    .name = "HeartBeatTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for BluetoothTask */
osThreadId_t BluetoothTaskHandle;
const osThreadAttr_t BluetoothTask_attributes = {
    .name = "BluetoothTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for UITask */
osThreadId_t UITaskHandle;
const osThreadAttr_t UITask_attributes = {
    .name = "UITask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for KeyHandleTask */
osThreadId_t KeyHandleTaskHandle;
const osThreadAttr_t KeyHandleTask_attributes = {
    .name = "KeyHandleTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for ADCHandleTask */
osThreadId_t ADCHandleTaskHandle;
const osThreadAttr_t ADCHandleTask_attributes = {
    .name = "ADCHandleTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for ServoHandleTask */
osThreadId_t ServoHandleTaskHandle;
const osThreadAttr_t ServoHandleTask_attributes = {
    .name = "ServoHandleTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for HandleBtMsgTask */
osThreadId_t HandleBtMsgTaskHandle;
const osThreadAttr_t HandleBtMsgTask_attributes = {
    .name = "HandleBtMsgTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for HandleCloudMsgT */
osThreadId_t HandleCloudMsgTHandle;
const osThreadAttr_t HandleCloudMsgT_attributes = {
    .name = "HandleCloudMsgT",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for CloudRxQueue */
osMessageQueueId_t CloudRxQueueHandle;
const osMessageQueueAttr_t CloudRxQueue_attributes = {
    .name = "CloudRxQueue"};
/* Definitions for CloudTxQueue */
osMessageQueueId_t CloudTxQueueHandle;
const osMessageQueueAttr_t CloudTxQueue_attributes = {
    .name = "CloudTxQueue"};
/* Definitions for BtRxQueue */
osMessageQueueId_t BtRxQueueHandle;
const osMessageQueueAttr_t BtRxQueue_attributes = {
    .name = "BtRxQueue"};
/* Definitions for BtTxQueue */
osMessageQueueId_t BtTxQueueHandle;
const osMessageQueueAttr_t BtTxQueue_attributes = {
    .name = "BtTxQueue"};
/* Definitions for WifiCfgQueue */
osMessageQueueId_t WifiCfgQueueHandle;
const osMessageQueueAttr_t WifiCfgQueue_attributes = {
    .name = "WifiCfgQueue"};
/* Definitions for StateMutex */
osMutexId_t StateMutexHandle;
const osMutexAttr_t StateMutex_attributes = {
    .name = "StateMutex"};
/* Definitions for DebugMutex */
osMutexId_t DebugMutexHandle;
const osMutexAttr_t DebugMutex_attributes = {
    .name = "DebugMutex"};
/* Definitions for KeyEventFlags */
osEventFlagsId_t KeyEventFlagsHandle;
const osEventFlagsAttr_t KeyEventFlags_attributes = {
    .name = "KeyEventFlags"};
/* Definitions for NetEventFlags */
osEventFlagsId_t NetEventFlagsHandle;
const osEventFlagsAttr_t NetEventFlags_attributes = {
    .name = "NetEventFlags"};
/* USER CODE BEGIN PV */
uint8_t cloud_armed = 0;                      // 布防状态标志：1=已布防，0=已撤防
uint8_t local_alarm = 0;                      // 本地报警状态：1=报警中，0=无报警
uint8_t alarm_muted = 0;                      // 报警消音标志：1=已消音，0=未消音
uint8_t led1_on = 0;                          // LED1开关状态：1=开，0=关
uint8_t beep_on = 0;                          // 蜂鸣器开关状态：1=开，0=关
uint16_t light_value = 0;                     // 光敏传感器ADC读数（值越大光线越暗）
uint16_t reference_value = 0;                 // 阈值电位器ADC读数（与光敏值比较触发报警）
uint16_t last_print_light_value = 0;          // 上一次打印的光敏值
uint16_t last_print_reference_value = 0;      // 上一次打印的阈值值
uint8_t adc_print_first_time = 1;             // 1表示还没有打印过ADC值
uint8_t adc_value_changed = 0;                // 1表示本次ADC值需要打印
BtConfig_Data wifi_config;                    // 用于存储 Wi-Fi 配置
char ack_message[BEMFA_MESSAGE_MAX_LENGTH];   // 用于拼装云端控制回执
uint8_t wifi_config_pending = 0;              // 1表示正在等待 ESP-01S 配置 Wi-Fi
uint8_t wifi_result;                          // 用于存储 ESP-01S 配置结果
uint8_t ui_terminal_mode = 0;                 // OLED 界面模式：0=前台表情，1=后台终端

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
void StartDefaultTask(void *argument);
void NetWorkTask_Function(void *argument);
void HeartBeatTask_Function(void *argument);
void BluetoothTask_Function(void *argument);
void UITask_Function(void *argument);
void KeyHandleTask_Function(void *argument);
void ADCHandleTask_Function(void *argument);
void ServoHandleTask_Function(void *argument);
void HandleBtMsgTask_Function(void *argument);
void HandleCloudMsgTask_Function(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int fputc(int ch, FILE *f)
{
  uint8_t mutex_locked = 0;

  (void)f;

  /*
   * 系统启动阶段 FreeRTOS 还没有运行，
   * 此时不能使用互斥量。
   */
  if ((DebugMutexHandle != NULL) &&
      (osKernelGetState() == osKernelRunning))
  {
    if (osMutexAcquire(DebugMutexHandle, osWaitForever) == osOK)
    {
      mutex_locked = 1;
    }
  }

  /* 串口1作为调试输出口，多个任务打印时使用 DebugMutex 串行化。 */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);

  /* 同步到 OLED 后台终端缓存；真正绘制由 UITask 完成。 */
  UI_Terminal_PutChar((char)ch);

  if (mutex_locked)
  {
    osMutexRelease(DebugMutexHandle);
  }

  return ch;
}
/*
 * 云端控制回执：只把回执放入 CloudTxQueue。
 * 真正的巴法云发送统一由 NetWorkTask 完成，避免多个任务同时操作 BEMFA。
 */
static void CloudAck(const char *msg)
{
  char tx_msg[BEMFA_MESSAGE_MAX_LENGTH];

  if (msg == NULL)
  {
    return;
  }

  snprintf(tx_msg, sizeof(tx_msg), "%s", msg);

  if (osMessageQueuePut(CloudTxQueueHandle, tx_msg, 0, 0) == osOK)
  {
    printf("回执已进入发送队列: %s\r\n", tx_msg);
  }
  else
  {
    printf("回执队列已满，丢弃: %s\r\n", tx_msg);
  }
}

/*
 * 蓝牙回复统一进入 BtTxQueue。
 * 真正发送由 BluetoothTask 完成，避免多个任务同时操作 HC-05。
 */
static void BtReply(const char *msg)
{
  char tx_msg[64];

  if (msg == NULL)
  {
    return;
  }

  snprintf(tx_msg, sizeof(tx_msg), "%s", msg);

  if (osMessageQueuePut(BtTxQueueHandle, tx_msg, 0, 0) != osOK)
  {
    printf("蓝牙发送队列已满，丢弃: %s\r\n", tx_msg);
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
  MX_ADC1_Init();
  MX_ADC3_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
  /* ===========================================用户初始化============================================= */
  IIC_OLED_Init();

  /* 界面默认进入前台表情模式，KEY3 可切换到后台终端 */
  ui_terminal_mode = 0;
  UI_Preview_Init();
  /* 关闭蜂鸣器，熄灭所有 LED */
  BEEP_Off();

  LED_Off(LED_ID_0);
  LED_Off(LED_ID_1);
  LED_Off(LED_ID_2);
  LED_Off(LED_ID_3);

  /* 启动舵机 PWM，初始为撤防模式（0 度） */
  SERVO_Start();
  SERVO_SetMode(SERVO_MODE_DISARM);

  printf("Basic driver test start\r\n");
  HC05_StartReceive();   // 启动 HC-05 串口接收
  ESP01S_StartReceive(); // 启动 ESP-01S 串口接收
  // HAL_TIM_Base_Start_IT(&htim2); // 启动30秒定时器中断
  // HAL_TIM_Base_Start_IT(&htim6); // 启动0.5秒定时器中断
  // HAL_TIM_Base_Start_IT(&htim7); // 启动5毫秒定时器中断（系统节拍）

  /* 上电时先尝试读取上一次保存的 Wi-Fi 配置。 */
  if (at24c02_init() == 0)
  {
    if (WifiStorage_Load(&wifi_config))
    {
      printf("读取到已保存的 Wi-Fi 配置：%s\r\n", wifi_config.ssid);
      ESP01S_RequestWiFi(wifi_config.ssid, wifi_config.password);
    }
    else
    {
      printf("AT24C02 中没有有效的 Wi-Fi 配置\r\n");
    }
  }
  else
  {
    printf("AT24C02 未检测到，等待蓝牙配网\r\n");
  }

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of StateMutex */
  StateMutexHandle = osMutexNew(&StateMutex_attributes);

  /* creation of DebugMutex */
  DebugMutexHandle = osMutexNew(&DebugMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of CloudRxQueue */
  CloudRxQueueHandle = osMessageQueueNew(4, 128, &CloudRxQueue_attributes);

  /* creation of CloudTxQueue */
  CloudTxQueueHandle = osMessageQueueNew(2, 128, &CloudTxQueue_attributes);

  /* creation of BtRxQueue */
  BtRxQueueHandle = osMessageQueueNew(2, 128, &BtRxQueue_attributes);

  /* creation of BtTxQueue */
  BtTxQueueHandle = osMessageQueueNew(4, 64, &BtTxQueue_attributes);

  /* creation of WifiCfgQueue */
  WifiCfgQueueHandle = osMessageQueueNew(1, 64, &WifiCfgQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  /* defaultTask 当前没有业务，保留函数模板但不再创建任务。 */
  // defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of NetWorkTask */
  NetWorkTaskHandle = osThreadNew(NetWorkTask_Function, NULL, &NetWorkTask_attributes);

  /* creation of HeartBeatTask */
  HeartBeatTaskHandle = osThreadNew(HeartBeatTask_Function, NULL, &HeartBeatTask_attributes);

  /* creation of BluetoothTask */
  BluetoothTaskHandle = osThreadNew(BluetoothTask_Function, NULL, &BluetoothTask_attributes);

  /* creation of UITask */
  UITaskHandle = osThreadNew(UITask_Function, NULL, &UITask_attributes);

  /* creation of KeyHandleTask */
  KeyHandleTaskHandle = osThreadNew(KeyHandleTask_Function, NULL, &KeyHandleTask_attributes);

  /* creation of ADCHandleTask */
  ADCHandleTaskHandle = osThreadNew(ADCHandleTask_Function, NULL, &ADCHandleTask_attributes);

  /* creation of ServoHandleTask */
  ServoHandleTaskHandle = osThreadNew(ServoHandleTask_Function, NULL, &ServoHandleTask_attributes);

  /* creation of HandleBtMsgTask */
  HandleBtMsgTaskHandle = osThreadNew(HandleBtMsgTask_Function, NULL, &HandleBtMsgTask_attributes);

  /* creation of HandleCloudMsgT */
  HandleCloudMsgTHandle = osThreadNew(HandleCloudMsgTask_Function, NULL, &HandleCloudMsgT_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of KeyEventFlags */
  KeyEventFlagsHandle = osEventFlagsNew(&KeyEventFlags_attributes);

  /* creation of NetEventFlags */
  NetEventFlagsHandle = osEventFlagsNew(&NetEventFlags_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  /*   while (1)
    {
    } */
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
   */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV6;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
   */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */
}

/**
 * @brief ADC3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
   */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV6;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = DISABLE;
  hadc3.Init.ContinuousConvMode = ENABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
   */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 167;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 19999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 8399;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 299999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8399;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 4999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */
}

/**
 * @brief TIM7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 8399;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 49;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, LED0_Pin | LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LED2_Pin | LED3_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, IIC_SOFT_SCL_Pin | IIC_SOFT_SDA_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : KEY1_Pin KEY2_Pin KEY3_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin | KEY2_Pin | KEY3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BEEP_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED0_Pin LED1_Pin */
  GPIO_InitStruct.Pin = LED0_Pin | LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY0_Pin */
  GPIO_InitStruct.Pin = KEY0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin LED3_Pin */
  GPIO_InitStruct.Pin = LED2_Pin | LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : IIC_SOFT_SCL_Pin IIC_SOFT_SDA_Pin */
  GPIO_InitStruct.Pin = IIC_SOFT_SCL_Pin | IIC_SOFT_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : HC05_STATE_Pin */
  GPIO_InitStruct.Pin = HC05_STATE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HC05_STATE_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /*
   * defaultTask 由 CubeMX 默认生成，当前业务已经拆分到各专用任务。
   * 这里低频空跑，后续可在 CubeMX 中删除该任务。
   */
  for (;;)
  {
    osDelay(1000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_NetWorkTask_Function */
/**
 * @brief Function implementing the NetWorkTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_NetWorkTask_Function */
void NetWorkTask_Function(void *argument)
{
  /* USER CODE BEGIN NetWorkTask_Function */
  char cloud_msg[BEMFA_MESSAGE_MAX_LENGTH];    // 接收缓冲：从 CloudRxQueue 取出的消息
  char cloud_tx_msg[BEMFA_MESSAGE_MAX_LENGTH]; // 发送缓冲：从 CloudTxQueue 取出的消息
  WifiCfgQueueItem wifi_req;                   // Wi-Fi 配置队列中的项
  uint8_t cloud_connected_before_local = 0;

  for (;;)
  {
    /* 蓝牙配网结果交给网络任务，由网络任务统一操作 ESP8266 */
    if (osMessageQueueGet(WifiCfgQueueHandle, &wifi_req, NULL, 0) == osOK)
    {
      wifi_config = wifi_req.data;

      printf("正在连接 WiFi，名称：%s\r\n", wifi_config.ssid);

      if (ESP01S_RequestWiFi(wifi_config.ssid, wifi_config.password))
      {
        wifi_config_pending = 1;
        printf("Wi-Fi 连接请求已提交\r\n");
      }
      else
      {
        wifi_config_pending = 0;
        BtReply("WIFI FAIL\r\n");
        printf("Wi-Fi 配置不合法，未提交连接请求\r\n");
      }
    }
    /* 处理 ESP8266 连接结果 ，根据结果更新 at24C02 中的 Wi-Fi 配置状态 */
    if (ESP01S_TakeWiFiResult(&wifi_result))
    {
      if (wifi_result)
      {
        BtReply("WIFI OK\r\n");
        printf("WiFi 连接成功\r\n");

        if (wifi_config_pending)
        {
          if (WifiStorage_Save(&wifi_config))
          {
            printf("Wi-Fi 配置保存成功\r\n");
          }
          else
          {
            printf("Wi-Fi 配置保存失败\r\n");
          }

          wifi_config_pending = 0;
        }
      }
      else if (wifi_config_pending)
      {
        BtReply("WIFI FAIL\r\n");
        printf("WiFi 连接失败，保留原来的 EEPROM 配置\r\n");
        wifi_config_pending = 0;
      }
    }
    /* 推进 ESP8266 状态机 */
    ESP01S_Service();

    if (ESP01S_IsWiFiConnected())
    {
      if (!BEMFA_IsConnected())
      {
        BEMFA_RequestConnect();
      }
      /* 推进巴法云状态机 */
      BEMFA_Service();
    }
    else
    {
      BEMFA_OnWiFiDisconnected();
    }
    if (BEMFA_IsConnected() && !cloud_connected_before_local)
    {
      cloud_connected_before_local = 1;
      printf("巴法云连接成功，主题：%s\r\n", BEMFA_TOPIC);
    }

    if (!BEMFA_IsConnected())
    {
      cloud_connected_before_local = 0;
    }

    /* 发送心跳包 */
    if (osEventFlagsGet(NetEventFlagsHandle) & NET_EVT_HEARTBEAT)
    {
      osEventFlagsClear(NetEventFlagsHandle, NET_EVT_HEARTBEAT);

      if (BEMFA_IsConnected())
      {
        BEMFA_SendHeartbeat();
      }
    }

    /* 只有网络任务可以真正调用 BEMFA_Publish()，避免多个任务抢巴法云状态机 */
    if (BEMFA_IsConnected())
    {
      if (osMessageQueueGet(CloudTxQueueHandle, cloud_tx_msg, NULL, 0) == osOK)
      {
        if (BEMFA_Publish(cloud_tx_msg) == 0)
        {
          printf("云端发送失败: %s\r\n", cloud_tx_msg);
        }
      }
    }
    /* 读取巴法云状态机发送的消息 */
    if (BEMFA_ReadMessage(cloud_msg, sizeof(cloud_msg)))
    {
      osMessageQueuePut(CloudRxQueueHandle, cloud_msg, 0, 0);
    }

    osDelay(5);
  }
  /* USER CODE END NetWorkTask_Function */
}

/* USER CODE BEGIN Header_HeartBeatTask_Function */
/**
 * @brief Function implementing the HeartBeatTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_HeartBeatTask_Function */
void HeartBeatTask_Function(void *argument)
{
  /* USER CODE BEGIN HeartBeatTask_Function */
  for (;;)
  {
    osDelay(30000);
    osEventFlagsSet(NetEventFlagsHandle, NET_EVT_HEARTBEAT);
  }
  /* USER CODE END HeartBeatTask_Function */
}

/* USER CODE BEGIN Header_BluetoothTask_Function */
/**
 * @brief Function implementing the BluetoothTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_BluetoothTask_Function */
void BluetoothTask_Function(void *argument)
{
  /* USER CODE BEGIN BluetoothTask_Function */
  char rx_line[128]; // 蓝牙接收到的一行数据
  char tx_msg[64];   // 蓝牙发送的一行数据

  for (;;)
  {
    /* 发送其他任务提交的蓝牙回复 */
    while (osMessageQueueGet(BtTxQueueHandle, tx_msg, NULL, 0) == osOK)
    {
      HC05_SendString(tx_msg);
    }

    /* 读取 HC-05 已经接收完成的一整行 */
    if (HC05_ReadLine(rx_line, sizeof(rx_line)))
    {
      if (osMessageQueuePut(BtRxQueueHandle, rx_line, 0, 0) != osOK)
      {
        printf("蓝牙接收队列已满，丢弃: %s\r\n", rx_line);
      }
    }

    osDelay(5);
  }
  /* USER CODE END BluetoothTask_Function */
}

/* USER CODE BEGIN Header_UITask_Function */
/**
 * @brief Function implementing the UITask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_UITask_Function */
void UITask_Function(void *argument)
{
  /* USER CODE BEGIN UITask_Function */
  uint8_t last_ui_terminal_mode = ui_terminal_mode;
  uint8_t current_mode = 0;
  uint8_t current_armed = 0;
  uint8_t current_alarm = 0;
  uint8_t current_light = 0;

  for (;;)
  {
    /* 蓝牙连接状态来自 HC05 STATE 引脚：连接时为高电平 */
    uint8_t bt_connected =
        (HAL_GPIO_ReadPin(HC05_STATE_GPIO_Port, HC05_STATE_Pin) == GPIO_PIN_SET)
            ? 1u
            : 0u;

    UI_Preview_SetBt(bt_connected);
    UI_Terminal_SetBt(bt_connected);

    /*
     * 共享状态先复制到局部变量，避免 OLED 刷新过程中长时间占用 StateMutex。
     */
    if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
    {
      current_mode = ui_terminal_mode;
      current_armed = cloud_armed;
      current_alarm = local_alarm;
      current_light = (light_value <= reference_value) ? 1u : 0u;
      osMutexRelease(StateMutexHandle);
    }

    UI_Preview_SetWifi(ESP01S_IsWiFiConnected());
    UI_Preview_SetCloud(BEMFA_IsConnected());
    UI_Preview_SetArmed(current_armed);
    UI_Preview_SetAlarm(current_alarm);
    UI_Preview_SetLight(current_light);

    UI_Terminal_SetWifi(ESP01S_IsWiFiConnected());
    UI_Terminal_SetCloud(BEMFA_IsConnected());
    UI_Terminal_SetArmed(current_armed);
    UI_Terminal_SetLight(current_light);

    /*
     * OLED 初始化和刷新都放在 UITask 中，
     * 避免多个任务同时操作软件 I2C 导致卡死。
     */
    if (current_mode != last_ui_terminal_mode)
    {
      last_ui_terminal_mode = current_mode;

      if (osMutexAcquire(DebugMutexHandle, osWaitForever) == osOK)
      {
        if (current_mode == 0)
        {
          UI_Preview_Init();
        }
        else
        {
          UI_Terminal_Init();
        }
        osMutexRelease(DebugMutexHandle);
      }

      if (current_mode == 0)
      {
        printf("已切换到前台表情界面\r\n");
      }
      else
      {
        printf("已切换到后台终端界面\r\n");
      }
    }

    if (osMutexAcquire(DebugMutexHandle, osWaitForever) == osOK)
    {
      if (current_mode == 0)
      {
        UI_Preview_Service();
      }
      else
      {
        UI_Terminal_Service();
      }

      osMutexRelease(DebugMutexHandle);
    }

    osDelay(5);
  }
  /* USER CODE END UITask_Function */
}

/* USER CODE BEGIN Header_KeyHandleTask_Function */
/**
 * @brief Function implementing the KeyHandleTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_KeyHandleTask_Function */
void KeyHandleTask_Function(void *argument)
{
  /* USER CODE BEGIN KeyHandleTask_Function */
  uint32_t flags;
  uint8_t armed_after;
  uint8_t alarm_active;
  uint8_t led1_after;
  uint8_t ui_mode_after;

  for (;;)
  {
    flags = osEventFlagsWait(
        KeyEventFlagsHandle,
        KEY_EVT_KEY0 | KEY_EVT_KEY1 | KEY_EVT_KEY2 | KEY_EVT_KEY3,
        osFlagsWaitAny,
        osWaitForever);

    if ((flags & KEY_EVT_KEY0) != 0)
    {
      armed_after = 0;

      if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
      {
        if (cloud_armed == 0)
        {
          cloud_armed = 1;
          armed_after = 1;
        }
        else
        {
          cloud_armed = 0;
          local_alarm = 0;
          alarm_muted = 0;
          beep_on = 0;
          armed_after = 0;
        }

        osMutexRelease(StateMutexHandle);
      }

      if (armed_after)
      {
        SERVO_SetMode(SERVO_MODE_SCAN);
        printf("KEY0：设备已布防\r\n");
      }
      else
      {
        LED_Off(LED_ID_0);
        BEEP_Off();
        SERVO_SetMode(SERVO_MODE_DISARM);

        printf("KEY0：设备已撤防\r\n");
      }
    }

    if ((flags & KEY_EVT_KEY1) != 0)
    {
      alarm_active = 0;

      if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
      {
        if (local_alarm)
        {
          alarm_muted = 1;
          beep_on = 0;
          alarm_active = 1;
        }
        osMutexRelease(StateMutexHandle);
      }

      if (alarm_active)
      {
        BEEP_Off();
        printf("KEY1：报警已消音，报警状态保持\r\n");
      }
      else
      {
        printf("KEY1：当前没有报警\r\n");
      }
    }

    if ((flags & KEY_EVT_KEY2) != 0)
    {
      led1_after = 0;

      if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
      {
        led1_on = !led1_on;
        led1_after = led1_on;
        osMutexRelease(StateMutexHandle);
      }

      if (led1_after)
      {
        LED_On(LED_ID_1);
        printf("KEY2：LED1 已打开\r\n");
      }
      else
      {
        LED_Off(LED_ID_1);
        printf("KEY2：LED1 已关闭\r\n");
      }
    }

    if ((flags & KEY_EVT_KEY3) != 0)
    {
      ui_mode_after = 0;

      if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
      {
        ui_terminal_mode = (ui_terminal_mode == 0) ? 1 : 0;
        ui_mode_after = ui_terminal_mode;
        osMutexRelease(StateMutexHandle);
      }

      if (ui_mode_after == 0)
      {
        printf("KEY3：请求切换到前台表情界面\r\n");
      }
      else
      {
        printf("KEY3：请求切换到后台终端界面\r\n");
      }
    }
  }
  /* USER CODE END KeyHandleTask_Function */
}

/* USER CODE BEGIN Header_ADCHandleTask_Function */
/**
 * @brief Function implementing the ADCHandleTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_ADCHandleTask_Function */
void ADCHandleTask_Function(void *argument)
{
  /* USER CODE BEGIN ADCHandleTask_Function */

  uint16_t new_light_value;
  uint16_t new_reference_value;

  uint8_t light_is_dark;
  uint8_t alarm_started;
  uint8_t alarm_cleared;
  uint8_t armed_now;

  char alarm_message[BEMFA_MESSAGE_MAX_LENGTH];

  for (;;)
  {
    alarm_started = 0;
    alarm_cleared = 0;
    armed_now = 0;
    light_is_dark = 0;

    /* 读取光敏和电位器 */
    new_light_value = ADC_ReadLight();
    new_reference_value = ADC_ReadReference();

    /*
     * 只有ADC变化超过阈值时才打印。
     * 如果你希望阈值为50，请确认宏定义为：
     * #define ADC_PRINT_CHANGE_THRESHOLD 50U
     */
    adc_value_changed = adc_print_first_time;

    if ((new_light_value > last_print_light_value &&
         new_light_value - last_print_light_value >
             ADC_PRINT_CHANGE_THRESHOLD) ||
        (last_print_light_value > new_light_value &&
         last_print_light_value - new_light_value >
             ADC_PRINT_CHANGE_THRESHOLD) ||
        (new_reference_value > last_print_reference_value &&
         new_reference_value - last_print_reference_value >
             ADC_PRINT_CHANGE_THRESHOLD) ||
        (last_print_reference_value > new_reference_value &&
         last_print_reference_value - new_reference_value >
             ADC_PRINT_CHANGE_THRESHOLD))
    {
      adc_value_changed = 1;
    }

    if (adc_value_changed)
    {
      last_print_light_value = new_light_value;
      last_print_reference_value = new_reference_value;
      adc_print_first_time = 0;

      printf("光敏值=%u, 阈值=%u\r\n",
             new_light_value,
             new_reference_value);
    }

    /*
     * StateMutex只保护共享状态变量。
     * ADC读取和硬件控制不放在互斥量内部。
     */
    if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
    {
      light_value = new_light_value;
      reference_value = new_reference_value;

      armed_now = cloud_armed;
      light_is_dark =
          (new_light_value > new_reference_value) ? 1U : 0U;

      if (light_is_dark)
      {
        /*
         * 只在布防且当前没有报警时触发一次。
         */
        if (cloud_armed && local_alarm == 0)
        {
          local_alarm = 1;
          alarm_muted = 0;
          beep_on = 1;
          alarm_started = 1;
        }
      }
      else
      {
        /*
         * 光线恢复正常后解除报警。
         */
        if (local_alarm)
        {
          local_alarm = 0;
          alarm_muted = 0;
          beep_on = 0;
          alarm_cleared = 1;
        }
      }

      osMutexRelease(StateMutexHandle);
    }

    if (adc_value_changed)
    {
      if (light_is_dark)
      {
        printf("当前光线较暗\r\n");
      }
      else
      {
        printf("当前光线正常\r\n");
      }
    }

    if (alarm_started)
    {
      LED_On(LED_ID_0);
      BEEP_On();
      SERVO_SetMode(SERVO_MODE_ALARM);

      printf("光线过暗，触发报警\r\n");

      /*
       * 报警消息放入云端发送队列，
       * 由NetWorkTask统一调用BEMFA_Publish()。
       */
      snprintf(alarm_message,
               sizeof(alarm_message),
               "ALARM:LOW_LIGHT");

      if (osMessageQueuePut(
              CloudTxQueueHandle,
              alarm_message,
              0,
              0) != osOK)
      {
        printf("报警消息进入云端队列失败\r\n");
      }
      else
      {
        printf("低光报警已进入云端发送队列\r\n");
      }
    }

    if (alarm_cleared)
    {
      LED_Off(LED_ID_0);
      BEEP_Off();

      if (armed_now)
      {
        SERVO_SetMode(SERVO_MODE_SCAN);
      }
      else
      {
        SERVO_SetMode(SERVO_MODE_DISARM);
      }

      printf("光线恢复，解除报警\r\n");
    }

    /*
     * 每500 ms读取一次ADC。
     */
    osDelay(500);
  }

  /* USER CODE END ADCHandleTask_Function */
}

/* USER CODE BEGIN Header_ServoHandleTask_Function */
/**
 * @brief Function implementing the ServoHandleTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_ServoHandleTask_Function */
void ServoHandleTask_Function(void *argument)
{
  /* USER CODE BEGIN ServoHandleTask_Function */

  for (;;)
  {
    /*
     * SERVO_Service() 内部会根据 HAL_GetTick()
     * 判断是否到了 25 ms 的扫描时间。
     */
    SERVO_Service();

    /*
     * 任务每 5 ms 检查一次，
     * 足够满足舵机每 25 ms 改变一次角度的要求。
     */
    osDelay(5);
  }

  /* USER CODE END ServoHandleTask_Function */
}

/* USER CODE BEGIN Header_HandleBtMsgTask_Function */
/**
 * @brief Function implementing the HandleBtMsgTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_HandleBtMsgTask_Function */
void HandleBtMsgTask_Function(void *argument)
{
  /* USER CODE BEGIN HandleBtMsgTask_Function */
  char rx_line[128];
  CloudCmd_Face face;
  WifiCfgQueueItem wifi_item;
  uint8_t allow_face;

  for (;;)
  {
    if (osMessageQueueGet(BtRxQueueHandle, rx_line, NULL, osWaitForever) == osOK)
    {
      if (CloudCmd_ParseFace(rx_line, &face))
      {
        allow_face = 0;

        if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
        {
          allow_face = ((cloud_armed == 0) && (local_alarm == 0)) ? 1u : 0u;
          osMutexRelease(StateMutexHandle);
        }

        if (allow_face)
        {
          UI_Preview_SetManualFace((UI_Face)face);
          printf("蓝牙表情命令: %s\r\n", rx_line);
        }
        else
        {
          printf("表情命令仅在撤防且无报警时有效\r\n");
        }
      }
      else if (BtConfig_Parse(rx_line, &wifi_item.data))
      {
        BtReply("WIFI CONNECTING\r\n");

        if (osMessageQueuePut(WifiCfgQueueHandle, &wifi_item, 0, 0) == osOK)
        {
          printf("蓝牙收到 Wi-Fi 配置: %s\r\n", wifi_item.data.ssid);
        }
        else
        {
          BtReply("WIFI FAIL\r\n");
          printf("Wi-Fi 配置队列已满\r\n");
        }
      }
      else
      {
        BtReply("WIFI FAIL\r\n");
        printf("蓝牙消息格式错误: %s\r\n", rx_line);
      }
    }
  }
  /* USER CODE END HandleBtMsgTask_Function */
}

/* USER CODE BEGIN Header_HandleCloudMsgTask_Function */
/**
 * @brief Function implementing the HandleCloudMsgT thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_HandleCloudMsgTask_Function */
void HandleCloudMsgTask_Function(void *argument)
{
  /* USER CODE BEGIN HandleCloudMsgTask_Function */
  char rx_msg[BEMFA_MESSAGE_MAX_LENGTH];
  CloudCmd_Data cmd;
  uint8_t state_armed;
  uint8_t state_alarm;
  uint8_t state_led1;
  uint8_t state_beep;

  for (;;)
  {
    if (osMessageQueueGet(CloudRxQueueHandle, rx_msg, NULL, osWaitForever) == osOK)
    {
      printf("巴法云下发消息: %s\r\n", rx_msg);

      if (CloudCmd_Parse(rx_msg, &cmd))
      {
        if (cmd.type == CLOUD_CMD_ARM)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            cloud_armed = 1;
            osMutexRelease(StateMutexHandle);
          }

          SERVO_SetMode(SERVO_MODE_SCAN);
          printf("设备已布防\r\n");
          CloudAck("本地:ARM:OK");
        }
        else if (cmd.type == CLOUD_CMD_DISARM)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            cloud_armed = 0;
            local_alarm = 0;
            alarm_muted = 0;
            beep_on = 0;
            osMutexRelease(StateMutexHandle);
          }

          LED_Off(LED_ID_0);
          BEEP_Off();
          SERVO_SetMode(SERVO_MODE_DISARM);

          printf("设备已撤防\r\n");
          CloudAck("本地:DISARM:OK");
        }
        else if (cmd.type == CLOUD_CMD_LED_ON)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            led1_on = 1;
            osMutexRelease(StateMutexHandle);
          }

          LED_On(LED_ID_1);
          printf("LED1 已打开\r\n");
          CloudAck("本地:LED:ON");
        }
        else if (cmd.type == CLOUD_CMD_LED_OFF)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            led1_on = 0;
            osMutexRelease(StateMutexHandle);
          }

          LED_Off(LED_ID_1);
          printf("LED1 已关闭\r\n");
          CloudAck("本地:LED:OFF");
        }
        else if (cmd.type == CLOUD_CMD_BEEP_ON)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            beep_on = 1;
            osMutexRelease(StateMutexHandle);
          }

          BEEP_On();
          printf("蜂鸣器已打开\r\n");
          CloudAck("本地:BEEP:ON");
        }
        else if (cmd.type == CLOUD_CMD_BEEP_OFF)
        {
          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            beep_on = 0;
            osMutexRelease(StateMutexHandle);
          }

          BEEP_Off();
          printf("蜂鸣器已关闭\r\n");
          CloudAck("本地:BEEP:OFF");
        }
        else if (cmd.type == CLOUD_CMD_QUERY)
        {
          state_armed = 0;
          state_alarm = 0;
          state_led1 = 0;
          state_beep = 0;

          if (osMutexAcquire(StateMutexHandle, osWaitForever) == osOK)
          {
            state_armed = cloud_armed;
            state_alarm = local_alarm;
            state_led1 = led1_on;
            state_beep = beep_on;
            osMutexRelease(StateMutexHandle);
          }

          snprintf(ack_message, sizeof(ack_message),
                   "STATE:ARM=%u,ALARM=%u,LED=%u,BEEP=%u",
                   (unsigned int)state_armed,
                   (unsigned int)state_alarm,
                   (unsigned int)state_led1,
                   (unsigned int)state_beep);

          printf("当前状态：%s\r\n", ack_message);
          CloudAck(ack_message);
        }
      }
      else
      {
        printf("巴法云下发命令解析失败\r\n");
      }
    }
  }
  /* USER CODE END HandleCloudMsgTask_Function */
}

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
