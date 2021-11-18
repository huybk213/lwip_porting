/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <string.h>
#include <stdbool.h>

#define DEBUG_USART1_DMA        0
#define UART1_RX_BUFFER_SIZE    (512+256)
#define USE_DMA_TX              0

#if USE_DMA_TX
static lwrb_t m_ringbuffer_usart1_tx = 		// for GSM
{
    .buff = NULL,
};
static uint8_t m_usart1_tx_buffer[UART1_TX_BUFFER_SIZE];
static volatile bool m_usart1_tx_run = false;
#endif

static inline void usart1_hw_uart_rx_raw(uint8_t *data, uint32_t length);
uint8_t m_usart1_rx_buffer[UART1_RX_BUFFER_SIZE];
volatile uint32_t m_last_usart1_transfer_size = 0;
static bool m_usart1_is_enabled = true;
static volatile size_t m_old_usart1_dma_rx_pos;
/* USER CODE END 0 */

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  /**USART1 GPIO Configuration
  PA9   ------> USART1_TX
  PA10   ------> USART1_RX
  */
  GPIO_InitStruct.Pin = GSM_TX_Pin|GSM_RX_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USART1 DMA Init */

  /* USART1_RX Init */
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_2, LL_DMA_CHANNEL_4);

  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_2, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_2, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA2, LL_DMA_STREAM_2, LL_DMA_MODE_CIRCULAR);

  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_2, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_2, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_2, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_2, LL_DMA_MDATAALIGN_BYTE);

  LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_2);

  /* USART1_TX Init */
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_7, LL_DMA_CHANNEL_4);

  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_7, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_7, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA2, LL_DMA_STREAM_7, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_7, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_7, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_7, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_7, LL_DMA_MDATAALIGN_BYTE);

  LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_7);

  /* USART1 interrupt Init */
  NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(USART1_IRQn);

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART1, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART1);
  LL_USART_Enable(USART1);
  /* USER CODE BEGIN USART1_Init 2 */
//    LL_DMA_EnableIT_HT(DMA2, LL_DMA_STREAM_2);
    LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_2);
    LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_2);
//    LL_DMA_EnableIT_TC(DMA2, LL_DMA_CHANNEL_7);
//    LL_DMA_EnableIT_TE(DMA2, LL_DMA_CHANNEL_7);
    LL_USART_EnableIT_IDLE(USART1);
    usart1_hw_uart_rx_raw(m_usart1_rx_buffer, UART1_RX_BUFFER_SIZE);
  /* USER CODE END USART1_Init 2 */

}

/* USER CODE BEGIN 1 */
void usart1_start_dma_rx(void)
{
	usart1_hw_uart_rx_raw(m_usart1_rx_buffer, UART1_RX_BUFFER_SIZE);
}

void usart1_control(bool enable)
{	
	if (m_usart1_is_enabled == enable)
	{
		return;
	}
	
	m_usart1_is_enabled = enable;
	
	if (!m_usart1_is_enabled)
	{
        m_old_usart1_dma_rx_pos = 0;
        memset(m_usart1_rx_buffer, 0, sizeof(m_usart1_rx_buffer));
	}
	else
	{
        
	}
}

void usart1_tx_complete_callback(bool status)
{
}

void usart1_hw_uart_send_raw(uint8_t* raw, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        LL_USART_TransmitData8(USART1, raw[i]);
        while (0 == LL_USART_IsActiveFlag_TC(USART1));
    }
}

static volatile bool m_uart_rx_ongoing = false;
static inline void usart1_hw_uart_rx_raw(uint8_t *data, uint32_t length)
{
    NVIC_DisableIRQ(DMA2_Stream2_IRQn);
    if (!m_uart_rx_ongoing)
    {  
        m_uart_rx_ongoing = true;
        NVIC_EnableIRQ(DMA2_Stream2_IRQn);
                
        /* Enable DMA Channel Rx */
        LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_2);
    
        LL_DMA_ConfigAddresses(DMA2, LL_DMA_STREAM_2,
                              (uint32_t)&(USART1->DR),
                             (uint32_t)data,
                             LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
        
        LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_2, length);
        
        /* Enable DMA RX Interrupt */
        LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_2);
        LL_USART_EnableDMAReq_RX(USART1);

    }
    else
    {
        NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    }
}


extern void gsm_hw_layer_uart_fill_rx(uint8_t *data, uint32_t length);
void usart1_rx_complete_callback(bool status)
{
    if (status)
    {
        size_t pos;

        /* Calculate current position in buffer */
        pos = UART1_RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA2, LL_DMA_STREAM_2);
        if (pos != m_old_usart1_dma_rx_pos) 
        {                       /* Check change in received data */
            if (pos > m_old_usart1_dma_rx_pos) 
            {   /* Current position is over previous one */
                /* We are in "linear" mode */
                /* Process data directly by subtracting "pointers" */
//                DEBUG_RAW("%.*s", pos - m_old_usart1_dma_rx_pos, &m_usart1_rx_buffer[m_old_usart1_dma_rx_pos]);
				gsm_hw_layer_uart_fill_rx(&m_usart1_rx_buffer[m_old_usart1_dma_rx_pos], pos-m_old_usart1_dma_rx_pos);
            } 
            else 
            {
                /* We are in "overflow" mode */
                /* First process data to the end of buffer */
                /* Check and continue with beginning of buffer */
                gsm_hw_layer_uart_fill_rx(&m_usart1_rx_buffer[m_old_usart1_dma_rx_pos], UART1_RX_BUFFER_SIZE - m_old_usart1_dma_rx_pos);

                if (pos > 0) 
                {
                    gsm_hw_layer_uart_fill_rx(&m_usart1_rx_buffer[0], pos);
                }
            }
            m_old_usart1_dma_rx_pos = pos;                          /* Save current position as old */
        }
//        m_uart_rx_ongoing = false;
    }
    else
    {
        m_uart_rx_ongoing = false;
    }
}

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
