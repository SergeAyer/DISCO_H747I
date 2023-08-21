// Copyright 2022 Haute école d'ingénierie et d'architecture de Fribourg
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/****************************************************************************
 * @file lcd_display.cpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief LCD Display (STM32)
 *
 * @date 2023-07-05
 * @version 0.0.1
 ***************************************************************************/

#include "lcd_display.hpp"

// from DISCO_H747I/Drivers/BSP/STM32H747I-DISCO
#include "stm32h747i_discovery_bus.h"
#include "stm32h747i_discovery_sdram.h"

// from DISCO_H747I/Drivers/STM32H7xx_HAL_Driver
#include "mbed_trace.h"
#include "stdio.h"
#include "stm32h7xx_hal_dsi.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "LCDDisplay"
#endif  // MBED_CONF_MBED_TRACE_ENABLE

extern LTDC_HandleTypeDef hlcd_ltdc;
extern DSI_HandleTypeDef hlcd_dsi;

namespace disco {

// TO BE REMOVED
// static constexpr uint8_t pColLeft_[]    = {0x00, 0x00, 0x01, 0x8F}; /*   0 -> 399 */
// static constexpr uint8_t pColRight_[]   = {0x01, 0x90, 0x03, 0x1F}; /* 400 -> 799 */
static constexpr uint8_t pCol_[]  = {0x00, 0x00, 0x03, 0x1F}; /*   0 -> 799 */
static constexpr uint8_t pPage_[] = {
    0x00, 0x00, 0x03, 0x1F};  //= {0x00, 0x00, 0x01, 0xDF}; /*   0 -> 479 */
static constexpr uint8_t pSyncLeft_[] = {0x02, 0x15}; /* Scan @ 533 */
static constexpr uint32_t HACT        = 800; /* !!!! SCREEN DIVIDED INTO 2 AREAS !!!! */

#define CONVERTARGB88882RGB565(Color)                   \
    ((((Color & 0xFFU) >> 3) & 0x1FU) |                 \
     (((((Color & 0xFF00U) >> 8) >> 2) & 0x3FU) << 5) | \
     (((((Color & 0xFF0000U) >> 16) >> 3) & 0x1FU) << 11))

#define CONVERTRGB5652ARGB8888(Color)                        \
    (((((((Color >> 11) & 0x1FU) * 527) + 23) >> 6) << 16) | \
     ((((((Color >> 5) & 0x3FU) * 259) + 33) >> 6) << 8) |   \
     ((((Color & 0x1FU) * 527) + 23) >> 6) | 0xFF000000)

/**
 * @brief  Configure the MPU attributes as Write Through for External SDRAM.
 * @note   The Base Address is 0xD0000000 .
 *         The Configured Region Size is 32MB because same as SDRAM size.
 * @param  None
 * @retval None
 */
static void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct;

    /* Disable the MPU */
    HAL_MPU_Disable();

    /* Configure the MPU as Strongly ordered for not defined regions */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = 0x00;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Configure the MPU attributes as WT for SDRAM */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress      = SDRAM_DEVICE_ADDR;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_32MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Enable the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  Initializes the DSI LCD.
 * The ititialization is done as below:
 *     - DSI PLL ititialization
 *     - DSI ititialization
 *     - LTDC ititialization
 *     - OTM8009A LCD Display IC Driver ititialization
 * @param  None
 * @retval LCD state
 */
ReturnCode LCDDisplay::init() {
    /* Configure the MPU attributes as Write Through for SDRAM*/
    MPU_Config();

    // HAL_Init();
    /* Initialize the SDRAM */
    BSP_SDRAM_Init(0);
    /* Toggle Hardware Reset of the DSI LCD using
       its XRES signal (active low) */
    BSP_LCD_Reset(0);

    /* Call first MSP Initialize only in case of first initialization
     * This will set IP blocks LTDC, DSI and DMA2D
     * - out of reset
     * - clocked
     * - NVIC IRQ related to IP blocks enabled
     */
    mspInit();

    /* LCD clock configuration */
    /* LCD clock configuration */
    /* PLL3_VCO Input = HSE_VALUE/PLL3M = 5 Mhz */
    /* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 800 Mhz */
    /* PLLLCDCLK = PLL3_VCO Output/PLL3R = 800/19 = 42 Mhz */
    /* LTDC clock frequency = PLLLCDCLK = 42 Mhz */
    periphClkInitStruct_.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periphClkInitStruct_.PLL3.PLL3M           = 5;
    periphClkInitStruct_.PLL3.PLL3N           = 160;
    periphClkInitStruct_.PLL3.PLL3FRACN       = 0;
    periphClkInitStruct_.PLL3.PLL3P           = 2;
    periphClkInitStruct_.PLL3.PLL3Q           = 2;
    periphClkInitStruct_.PLL3.PLL3R           = 19;
    periphClkInitStruct_.PLL3.PLL3VCOSEL      = RCC_PLL3VCOWIDE;
    periphClkInitStruct_.PLL3.PLL3RGE         = RCC_PLL3VCIRANGE_2;

    HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct_);

    /* Base address of DSI Host/Wrapper registers to be set before calling De-Init */
    hlcd_dsi.Instance = DSI;

    HAL_DSI_DeInit(&(hlcd_dsi));

    dsiPllInit_.PLLNDIV = 100;
    dsiPllInit_.PLLIDF  = DSI_PLL_IN_DIV5;
    dsiPllInit_.PLLODF  = DSI_PLL_OUT_DIV1;

    hlcd_dsi.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
    hlcd_dsi.Init.TXEscapeCkdiv = 0x4;

    HAL_DSI_Init(&(hlcd_dsi), &(dsiPllInit_));

    /* Configure the DSI for Command mode */
    cmdCfg_.VirtualChannelID      = 0;
    cmdCfg_.HSPolarity            = DSI_HSYNC_ACTIVE_HIGH;
    cmdCfg_.VSPolarity            = DSI_VSYNC_ACTIVE_HIGH;
    cmdCfg_.DEPolarity            = DSI_DATA_ENABLE_ACTIVE_HIGH;
    cmdCfg_.ColorCoding           = DSI_RGB888;
    cmdCfg_.CommandSize           = HACT;
    cmdCfg_.TearingEffectSource   = DSI_TE_DSILINK;
    cmdCfg_.TearingEffectPolarity = DSI_TE_RISING_EDGE;
    cmdCfg_.VSyncPol              = DSI_VSYNC_FALLING;
    cmdCfg_.AutomaticRefresh      = DSI_AR_DISABLE;
    cmdCfg_.TEAcknowledgeRequest  = DSI_TE_ACKNOWLEDGE_ENABLE;
    HAL_DSI_ConfigAdaptedCommandMode(&hlcd_dsi, &cmdCfg_);

    lpCmd_.LPGenShortWriteNoP  = DSI_LP_GSW0P_ENABLE;
    lpCmd_.LPGenShortWriteOneP = DSI_LP_GSW1P_ENABLE;
    lpCmd_.LPGenShortWriteTwoP = DSI_LP_GSW2P_ENABLE;
    lpCmd_.LPGenShortReadNoP   = DSI_LP_GSR0P_ENABLE;
    lpCmd_.LPGenShortReadOneP  = DSI_LP_GSR1P_ENABLE;
    lpCmd_.LPGenShortReadTwoP  = DSI_LP_GSR2P_ENABLE;
    lpCmd_.LPGenLongWrite      = DSI_LP_GLW_ENABLE;
    lpCmd_.LPDcsShortWriteNoP  = DSI_LP_DSW0P_ENABLE;
    lpCmd_.LPDcsShortWriteOneP = DSI_LP_DSW1P_ENABLE;
    lpCmd_.LPDcsShortReadNoP   = DSI_LP_DSR0P_ENABLE;
    lpCmd_.LPDcsLongWrite      = DSI_LP_DLW_ENABLE;
    HAL_DSI_ConfigCommand(&hlcd_dsi, &lpCmd_);

    /* Initialize LTDC */
    ltdcInit();

    /* Start DSI */
    HAL_DSI_Start(&(hlcd_dsi));

    /* Configure DSI PHY HS2LP and LP2HS timings */
    DSI_PHY_TimerTypeDef phyTimings = {0};
    phyTimings.ClockLaneHS2LPTime   = 35;
    phyTimings.ClockLaneLP2HSTime   = 35;
    phyTimings.DataLaneHS2LPTime    = 35;
    phyTimings.DataLaneLP2HSTime    = 35;
    phyTimings.DataLaneMaxReadTime  = 0;
    phyTimings.StopWaitTime         = 10;
    HAL_DSI_ConfigPhyTimer(&hlcd_dsi, &phyTimings);

    /* Initialize the OTM8009A LCD Display IC Driver (KoD LCD IC Driver) */
    OTM8009A_IO_t ioCtx = {0};
    ioCtx.Address       = 0;
    ioCtx.GetTick       = BSP_GetTick;
    ioCtx.WriteReg      = DSI_IO_Write;
    ioCtx.ReadReg       = DSI_IO_Read;
    OTM8009A_RegisterBusIO(&otm8009AObj_, &ioCtx);
    lcd_CompObj_ = (&otm8009AObj_);
    OTM8009A_Init(lcd_CompObj_, OTM8009A_COLMOD_RGB888, LCD_ORIENTATION_LANDSCAPE);

    lpCmd_.LPGenShortWriteNoP  = DSI_LP_GSW0P_DISABLE;
    lpCmd_.LPGenShortWriteOneP = DSI_LP_GSW1P_DISABLE;
    lpCmd_.LPGenShortWriteTwoP = DSI_LP_GSW2P_DISABLE;
    lpCmd_.LPGenShortReadNoP   = DSI_LP_GSR0P_DISABLE;
    lpCmd_.LPGenShortReadOneP  = DSI_LP_GSR1P_DISABLE;
    lpCmd_.LPGenShortReadTwoP  = DSI_LP_GSR2P_DISABLE;
    lpCmd_.LPGenLongWrite      = DSI_LP_GLW_DISABLE;
    lpCmd_.LPDcsShortWriteNoP  = DSI_LP_DSW0P_DISABLE;
    lpCmd_.LPDcsShortWriteOneP = DSI_LP_DSW1P_DISABLE;
    lpCmd_.LPDcsShortReadNoP   = DSI_LP_DSR0P_DISABLE;
    lpCmd_.LPDcsLongWrite      = DSI_LP_DLW_DISABLE;
    HAL_DSI_ConfigCommand(&hlcd_dsi, &lpCmd_);

    HAL_DSI_ConfigFlowControl(&hlcd_dsi, DSI_FLOW_CONTROL_BTA);
    HAL_DSI_ForceRXLowPower(&hlcd_dsi, ENABLE);

    /* Set the LCD Context */
    // Lcd_Ctx is declared in "stm32h747i_discovery_lcd.h"
    Lcd_Ctx[0].ActiveLayer = currentLCDLayer_;
    Lcd_Ctx[0].PixelFormat = LCD_PIXEL_FORMAT_ARGB8888;
    Lcd_Ctx[0].BppFactor   = 4; /* 4 Bytes Per Pixel for ARGB8888 */
    Lcd_Ctx[0].XSize       = 800;
    Lcd_Ctx[0].YSize       = 480;

    /* Disable DSI Wrapper in order to access and configure the LTDC */
    __HAL_DSI_WRAPPER_DISABLE(&hlcd_dsi);

    /* Initialize LTDC layer 0 iused for Hint */
    LCD_LayerInit(currentLCDLayer_, LCD_FRAME_BUFFER);
    funcDriver_.GetXSize(0, &lcdXsize_);
    funcDriver_.GetYSize(0, &lcdYsize_);
    funcDriver_.GetFormat(0, &lcdPixelFormat_);

    /* Update pitch : the draw is done on the whole physical X Size */
    HAL_LTDC_SetPitch(&hlcd_ltdc, Lcd_Ctx[0].XSize, currentLCDLayer_);

    /* Enable DSI Wrapper so DSI IP will drive the LTDC */
    __HAL_DSI_WRAPPER_ENABLE(&hlcd_dsi);

    HAL_DSI_LongWrite(&hlcd_dsi,
                      0,
                      DSI_DCS_LONG_PKT_WRITE,
                      4,
                      OTM8009A_CMD_CASET,
                      // NOLINTNEXTLINE(readability/casting)
                      (uint8_t*)pCol_);
    HAL_DSI_LongWrite(&hlcd_dsi,
                      0,
                      DSI_DCS_LONG_PKT_WRITE,
                      4,
                      OTM8009A_CMD_PASET,
                      // NOLINTNEXTLINE(readability/casting)
                      (uint8_t*)pPage_);

    setFont(createFont36());

    fillDisplay(LCD_COLOR_WHITE);

    return ReturnCode::Ok;
}

void LCDDisplay::fillDisplay(uint32_t color) {
    fillRectangle(0, 0, lcdXsize_, lcdYsize_, color);
    refreshLCD();
}

void LCDDisplay::fillRectangle(
    uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height, uint32_t color) {
    fillRect(xPos, yPos, width, height, color);
    refreshLCD();
}

void LCDDisplay::displayPicture(
    const uint32_t* pSrc, uint16_t x, uint16_t y, uint16_t xsize, uint16_t ysize) {
    uint32_t destination = (uint32_t)LCD_FRAME_BUFFER + (y * 800 + x) * 4;
    uint32_t source      = (uint32_t)pSrc;

    /*##-1- Configure the DMA2D Mode, Color Mode and output offset #############*/
    hdma2d_.Init.Mode          = DMA2D_M2M;
    hdma2d_.Init.ColorMode     = DMA2D_OUTPUT_ARGB8888;
    hdma2d_.Init.OutputOffset  = 800 - xsize;
    hdma2d_.Init.AlphaInverted = DMA2D_REGULAR_ALPHA; /* No Output Alpha Inversion*/
    hdma2d_.Init.RedBlueSwap   = DMA2D_RB_REGULAR;    /* No Output Red & Blue swap */

    /*##-2- DMA2D Callbacks Configuration ######################################*/
    hdma2d_.XferCpltCallback = NULL;

    /*##-3- Foreground Configuration ###########################################*/
    hdma2d_.LayerCfg[currentLCDLayer_].AlphaMode      = DMA2D_REPLACE_ALPHA;
    hdma2d_.LayerCfg[currentLCDLayer_].InputAlpha     = 0x00;
    hdma2d_.LayerCfg[currentLCDLayer_].InputColorMode = DMA2D_INPUT_ARGB8888;
    hdma2d_.LayerCfg[currentLCDLayer_].InputOffset    = 0xFF;
    hdma2d_.LayerCfg[currentLCDLayer_].RedBlueSwap =
        DMA2D_RB_REGULAR; /* No ForeGround Red/Blue swap */
    hdma2d_.LayerCfg[currentLCDLayer_].AlphaInverted =
        DMA2D_INVERTED_ALPHA; /* No ForeGround Alpha inversion */

    hdma2d_.Instance = DMA2D;

    /* DMA2D Initialization */
    if (HAL_DMA2D_Init(&hdma2d_) == HAL_OK) {
        if (HAL_DMA2D_ConfigLayer(&hdma2d_, 1) == HAL_OK) {
            if (HAL_DMA2D_Start(&hdma2d_, source, destination, xsize, ysize) == HAL_OK) {
                /* Polling For DMA transfer */
                HAL_DMA2D_PollForTransfer(&hdma2d_, 100);
            }
        }
    }

    /* set the refresh area to LCD left half */
    HAL_DSI_LongWrite(&hlcd_dsi,
                      0,
                      DSI_DCS_LONG_PKT_WRITE,
                      2,
                      OTM8009A_CMD_WRTESCN,
                      // NOLINTNEXTLINE(readability/casting)
                      (uint8_t*)pSyncLeft_);

    /* Refresh the LCD */
    HAL_DSI_Refresh(&hlcd_dsi);
}

void LCDDisplay::refreshLCD() { HAL_DSI_Refresh(&hlcd_dsi); }

void LCDDisplay::mspInit() {
    /** @brief Enable the LTDC clock */
    __HAL_RCC_LTDC_CLK_ENABLE();

    /** @brief Toggle Sw reset of LTDC IP */
    __HAL_RCC_LTDC_FORCE_RESET();
    __HAL_RCC_LTDC_RELEASE_RESET();

    /** @brief Enable the DMA2D clock */
    __HAL_RCC_DMA2D_CLK_ENABLE();

    /** @brief Toggle Sw reset of DMA2D IP */
    __HAL_RCC_DMA2D_FORCE_RESET();
    __HAL_RCC_DMA2D_RELEASE_RESET();

    /** @brief Enable DSI Host and wrapper clocks */
    __HAL_RCC_DSI_CLK_ENABLE();

    /** @brief Soft Reset the DSI Host and wrapper */
    __HAL_RCC_DSI_FORCE_RESET();
    __HAL_RCC_DSI_RELEASE_RESET();

    /** @brief NVIC configuration for LTDC interrupt that is now enabled */
    HAL_NVIC_SetPriority(LTDC_IRQn, 9, 0xf);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);

    /** @brief NVIC configuration for DMA2D interrupt that is now enabled */
    HAL_NVIC_SetPriority(DMA2D_IRQn, 9, 0xf);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);

    /** @brief NVIC configuration for DSI interrupt that is now enabled */
    HAL_NVIC_SetPriority(DSI_IRQn, 9, 0xf);
    HAL_NVIC_EnableIRQ(DSI_IRQn);
}

/**
 * @brief  Initialize the LTDC
 * @param  None
 * @retval None
 */
void LCDDisplay::ltdcInit() {
    /* DeInit */
    hlcd_ltdc.Instance = LTDC;
    HAL_LTDC_DeInit(&hlcd_ltdc);

    /* LTDC Config */
    /* Timing and polarity */
    hlcd_ltdc.Init.HorizontalSync     = HSYNC;
    hlcd_ltdc.Init.VerticalSync       = VSYNC;
    hlcd_ltdc.Init.AccumulatedHBP     = HSYNC + HBP;
    hlcd_ltdc.Init.AccumulatedVBP     = VSYNC + VBP;
    hlcd_ltdc.Init.AccumulatedActiveH = VSYNC + VBP + VACT;
    hlcd_ltdc.Init.AccumulatedActiveW = HSYNC + HBP + HACT;
    hlcd_ltdc.Init.TotalHeigh         = VSYNC + VBP + VACT + VFP;
    hlcd_ltdc.Init.TotalWidth         = HSYNC + HBP + HACT + HFP;

    /* background value */
    hlcd_ltdc.Init.Backcolor.Blue  = 0;
    hlcd_ltdc.Init.Backcolor.Green = 0;
    hlcd_ltdc.Init.Backcolor.Red   = 0;

    /* Polarity */
    hlcd_ltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    hlcd_ltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    hlcd_ltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    hlcd_ltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
    hlcd_ltdc.Instance        = LTDC;

    HAL_LTDC_Init(&hlcd_ltdc);
}

/**
 * @brief  DCS or Generic short/long write command
 * @param  ChannelNbr Virtual channel ID
 * @param  Reg Register to be written
 * @param  pData pointer to a buffer of data to be write
 * @param  Size To precise command to be used (short or long)
 * @retval BSP status
 */

int32_t LCDDisplay::DSI_IO_Write(uint16_t channelNbr,
                                 uint16_t reg,
                                 uint8_t* pData,
                                 uint16_t size) {
    int32_t ret = BSP_ERROR_NONE;

    if (size <= 1U) {
        if (HAL_DSI_ShortWrite(&hlcd_dsi,
                               channelNbr,
                               DSI_DCS_SHORT_PKT_WRITE_P1,
                               reg,
                               (uint32_t)pData[size]) != HAL_OK) {
            ret = BSP_ERROR_BUS_FAILURE;
        }
    } else {
        if (HAL_DSI_LongWrite(&hlcd_dsi,
                              channelNbr,
                              DSI_DCS_LONG_PKT_WRITE,
                              size,
                              (uint32_t)reg,
                              pData) != HAL_OK) {
            ret = BSP_ERROR_BUS_FAILURE;
        }
    }

    return ret;
}

/**
 * @brief  DCS or Generic read command
 * @param  ChannelNbr Virtual channel ID
 * @param  Reg Register to be read
 * @param  pData pointer to a buffer to store the payload of a read back operation.
 * @param  Size  Data size to be read (in byte).
 * @retval BSP status
 */
int32_t LCDDisplay::DSI_IO_Read(uint16_t channelNbr,
                                uint16_t reg,
                                uint8_t* pData,
                                uint16_t size) {
    int32_t ret = BSP_ERROR_NONE;

    if (HAL_DSI_Read(
            &hlcd_dsi, channelNbr, pData, size, DSI_DCS_SHORT_PKT_READ, reg, pData) !=
        HAL_OK) {
        ret = BSP_ERROR_BUS_FAILURE;
    }

    return ret;
}

/**
 * @brief  Initializes the LCD layers.
 * @param  LayerIndex: Layer foreground or background
 * @param  FB_Address: Layer frame buffer
 * @retval None
 */
void LCDDisplay::LCD_LayerInit(uint16_t layerIndex, uint32_t address) {
    LTDC_LayerCfgTypeDef layercfg = {0};

    /* Layer Init */
    layercfg.WindowX0        = 0;
    layercfg.WindowX1        = Lcd_Ctx[0].XSize;
    layercfg.WindowY0        = 0;
    layercfg.WindowY1        = Lcd_Ctx[0].YSize;
    layercfg.PixelFormat     = LTDC_PIXEL_FORMAT_ARGB8888;
    layercfg.FBStartAdress   = address;
    layercfg.Alpha           = 255;
    layercfg.Alpha0          = 0;
    layercfg.Backcolor.Blue  = 0;
    layercfg.Backcolor.Green = 0;
    layercfg.Backcolor.Red   = 0;
    layercfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layercfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    layercfg.ImageWidth      = Lcd_Ctx[0].XSize;
    layercfg.ImageHeight     = Lcd_Ctx[0].YSize;

    HAL_LTDC_ConfigLayer(&hlcd_ltdc, &layercfg, layerIndex);
}

/**
 * @brief  Display Example description.
 * @param  None
 * @retval None
 */
void LCDDisplay::displayWelcome(const char* text, AlignMode alignMode) {
    fillDisplay(LCD_COLOR_WHITE);
    setTextColor(LCD_COLOR_BLUE);
    fillRect(0, 0, 800, 112, LCD_COLOR_BLUE);
    setTextColor(LCD_COLOR_WHITE);
    fillRect(0, 112, 800, 368, LCD_COLOR_WHITE);
    setBackColor(LCD_COLOR_BLUE);
    setFont(createFont36b());
    displayStringAtLine(1, "Welcome", alignMode);
    setTextColor(LCD_COLOR_BLUE);
    setBackColor(LCD_COLOR_WHITE);
    setFont(createFont36());
    displayStringAtLine(3, "to", alignMode);
    displayStringAtLine(5, text, alignMode);

    // setFont(&Font16);
    // displayStringAtLine(4, (uint8_t *)"This example shows how to display images on LCD
    // DSI using the partial"); displayStringAtLine(5, (uint8_t *)"Refresh method by
    // splitting the display area.");
    /* set the refresh area to LCD left half */

    HAL_DSI_LongWrite(&hlcd_dsi,
                      0,
                      DSI_DCS_LONG_PKT_WRITE,
                      2,
                      OTM8009A_CMD_WRTESCN,
                      // NOLINTNEXTLINE(readability/casting)
                      (uint8_t*)pSyncLeft_);

    /* Refresh the LCD */
    HAL_DSI_Refresh(&hlcd_dsi);
}

// DrawContext
/**
 * @brief  Sets the LCD text font.
 * @param  fonts  Layer font to be used
 */
void LCDDisplay::setFont(Font* pFont) { drawProp_[currentLCDLayer_].pFont = pFont; }

/**
 * @brief  Gets the LCD text font.
 * @retval Used layer font
 */
Font* LCDDisplay::getFont() { return drawProp_[currentLCDLayer_].pFont; }

/**
 * @brief  Gets the LCD width.
 * @retval LCD width
 */
uint32_t LCDDisplay::getWidth() const { return lcdXsize_; }

/**
 * @brief  Gets the LCD height
 * @retval LCD width
 */
uint32_t LCDDisplay::getHeight() const { return lcdYsize_; }

/**
 * @brief  Compute the line number depending on font size
 * @retval Line number to be used for display
 */
uint32_t LCDDisplay::computeDisplayLineNumber(uint32_t line) {
    return line * getFont()->height;
}

/**
 * @brief  Sets the LCD text color.
 * @param  color  Text color code
 */
void LCDDisplay::setTextColor(uint32_t color) {
    drawProp_[currentLCDLayer_].textColor = color;
}

/**
 * @brief  Sets the LCD background color.
 * @param  color  Layer background color code
 */
void LCDDisplay::setBackColor(uint32_t color) {
    drawProp_[currentLCDLayer_].backColor = color;
}

/**
 * @brief  Draws a full rectangle in currently active layer.
 * @param  xPos   X position
 * @param  yPos   Y position
 * @param  width  Rectangle width
 * @param  height Rectangle height
 * @param  color  Draw color
 */
void LCDDisplay::fillRect(
    uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height, uint32_t color) {
    /* Fill the rectangle */
    if (lcdPixelFormat_ == LCD_PIXEL_FORMAT_RGB565) {
        funcDriver_.FillRect(
            lcdDevice_, xPos, yPos, width, height, CONVERTARGB88882RGB565(color));
    } else {
        funcDriver_.FillRect(lcdDevice_, xPos, yPos, width, height, color);
    }
}
/**
 * @brief  Draws a RGB rectangle in currently active layer.
 * @param  pData   Pointer to RGB rectangle data
 * @param  xPos    X position
 * @param  yPos    Y position
 * @param  Length  Line length
 */
void LCDDisplay::fillRGBRect(
    uint32_t xPos, uint32_t yPos, uint8_t* pData, uint32_t width, uint32_t height) {
    /* Write RGB rectangle data */
    funcDriver_.FillRGBRect(lcdDevice_, xPos, yPos, pData, width, height);
}

/**
 * @brief  Displays a maximum of 60 characters on the LCD.
 * @param  line: Line where to display the character shape
 * @param  ptr: Pointer to string to display on LCD
 */
void LCDDisplay::displayStringAtLine(uint32_t line, const char* text, AlignMode mode) {
    displayStringAt(10, computeDisplayLineNumber(line), text, mode);
}

/**
 * @brief  Displays characters in currently active layer.
 * @param  xPos X position (in pixel)
 * @param  yPos Y position (in pixel)
 * @param  Text Pointer to string to display on LCD
 * @param  Mode Display mode
 *          This parameter can be one of the following values:
 *            @arg  CENTER_MODE
 *            @arg  RIGHT_MODE
 *            @arg  LEFT_MODE
 */
void LCDDisplay::displayStringAt(uint32_t xPos,
                                 uint32_t yPos,
                                 const char* text,
                                 AlignMode mode) {
    /* Get the text size */
    uint32_t nbrOfChars = 0;
    char* ptr           = const_cast<char*>(text);
    while (*ptr++) {
        nbrOfChars++;
    }

    /* Characters number per line */
    uint32_t nbrOfCharPerLine = (lcdXsize_ / drawProp_[currentLCDLayer_].pFont->width);
    uint32_t refcolumn        = 1;
    switch (mode) {
        case AlignMode::CENTER_MODE: {
            refcolumn = xPos + ((nbrOfCharPerLine - nbrOfChars) *
                                drawProp_[currentLCDLayer_].pFont->width) /
                                   2;
            break;
        }
        case AlignMode::LEFT_MODE: {
            refcolumn = xPos;
            break;
        }
        case AlignMode::RIGHT_MODE: {
            refcolumn = -xPos + ((nbrOfCharPerLine - nbrOfChars) *
                                 drawProp_[currentLCDLayer_].pFont->width);
            break;
        }
        default: {
            refcolumn = xPos;
            break;
        }
    }

    /* Check that the Start column is located in the screen */
    if ((refcolumn < 1) || (refcolumn >= 0x8000)) {
        refcolumn = 1;
    }

    /* Send the string character by character on LCD */
    uint32_t i = 0;
    while ((*text != 0) & (((lcdXsize_ - (i * drawProp_[currentLCDLayer_].pFont->width)) &
                            0xFFFF) >= drawProp_[currentLCDLayer_].pFont->width)) {
        // Display one character on LCD
        displayChar(refcolumn, yPos, *text);
        // Inclrement the column position by the width
        refcolumn += drawProp_[currentLCDLayer_].pFont->width;

        // Point on the next character
        text++;
        i++;
    }
}

/**
 * @brief  Displays one character in currently active layer.
 * @param  xPos Start column address
 * @param  yPos Line where to display the character shape.
 * @param  ascii Character ascii code
 *           This parameter must be a number between Min_Data = 0x20 and Max_Data = 0x7E
 */
void LCDDisplay::displayChar(uint32_t xPos, uint32_t yPos, uint8_t ascii) {
    uint32_t offsetInTable = (ascii - ' ') * drawProp_[currentLCDLayer_].pFont->height *
                             ((drawProp_[currentLCDLayer_].pFont->width + 7) / 8);
    drawChar(xPos, yPos, &drawProp_[currentLCDLayer_].pFont->table[offsetInTable]);
}

/**
 * @brief  Draws a character on LCD.
 * @param  xPos  Line where to display the character shape
 * @param  yPos  Start column address
 * @param  pData Pointer to the character data
 */
void LCDDisplay::drawChar(uint32_t xPos, uint32_t yPos, const uint8_t* pData) {
    uint32_t height = drawProp_[currentLCDLayer_].pFont->height;
    uint32_t width  = drawProp_[currentLCDLayer_].pFont->width;

    // compute the bit offset in each line
    uint32_t offset = 8 * ((width + 7) / 8) - width;

    // draw each line of the char stored in table
    for (uint32_t i = 0; i < height; i++) {
        // get the start address of the line
        uint32_t nbrOfBytesPerLine = (width + 7) / 8;
        uint8_t* pchar = (const_cast<uint8_t*>(pData) + nbrOfBytesPerLine * i);

        uint64_t line = 0;
        for (uint32_t i = 0; i < nbrOfBytesPerLine; i++) {
            line <<= 8;
            line |= pchar[i];
        }

        if (lcdPixelFormat_ == LCD_PIXEL_FORMAT_RGB565) {
            uint16_t rgb565[48] = {0};
            for (uint32_t j = 0; j < width; j++) {
                // check whether the j^th bit in line is on or off
                uint64_t bitInPixel = (uint64_t)1 << (uint64_t)(width - j + offset - 1);
                if (line & bitInPixel) {
                    rgb565[j] =
                        CONVERTARGB88882RGB565(drawProp_[currentLCDLayer_].textColor);
                } else {
                    rgb565[j] =
                        CONVERTARGB88882RGB565(drawProp_[currentLCDLayer_].backColor);
                }
            }

            fillRGBRect(xPos, yPos++, (uint8_t*)(&rgb565[0]), width, 1);  // NOLINT
        } else {
            uint32_t argb8888[48] = {0};
            for (uint32_t j = 0; j < width; j++) {
                // check whether the j^th bit in line is on or off
                uint64_t bitInPixel = (uint64_t)1 << (uint64_t)(width - j + offset - 1);
                if (line & bitInPixel) {
                    argb8888[j] = drawProp_[currentLCDLayer_].textColor;
                } else {
                    argb8888[j] = drawProp_[currentLCDLayer_].backColor;
                }
            }

            fillRGBRect(xPos, yPos++, (uint8_t*)(&argb8888[0]), width, 1);  // NOLINT
        }
    }
}

/**
 * @brief  Gets the LCD X size.
 * @param  Instance  LCD Instance
 * @param  XSize     LCD width
 * @retval BSP status
 */
int32_t LCDDisplay::getXSize(uint32_t instance, uint32_t* xSize) {
    *xSize = Lcd_Ctx[0].XSize;

    return BSP_ERROR_NONE;
}

/**
 * @brief  Gets the LCD Y size.
 * @param  Instance  LCD Instance
 * @param  YSize     LCD Height
 * @retval BSP status
 */
int32_t LCDDisplay::getYSize(uint32_t instance, uint32_t* ySize) {
    *ySize = Lcd_Ctx[0].YSize;

    return BSP_ERROR_NONE;
}

}  // namespace disco
