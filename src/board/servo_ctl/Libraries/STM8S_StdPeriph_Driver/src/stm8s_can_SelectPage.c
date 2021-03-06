
/**
  ********************************************************************************
  * @file    stm8s_can_SelectPage.c
  * @author  MCD Application Team
  * @version V2.2.0
  * @date    30-September-2014
  * @brief   This file contains all the functions for the UART1 peripheral.
   ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm8s_can.h"
#include "private/stm8s_can_private.h"

/** @addtogroup STM8S_StdPeriph_Driver
  * @{
  */

/** @}
  * @addtogroup CAN_Public_Functions
  * @{
  */

/**
  * @brief  Sets the registers page to be selected.
  * @param   the selected page which can be one of the @ref CAN_Page_TypeDef.
  * @retval None
  */
void CAN_SelectPage(CAN_Page_TypeDef CAN_Page)
{
  CAN->PSR = (uint8_t)CAN_Page;
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
