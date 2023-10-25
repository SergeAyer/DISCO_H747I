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
 * @file lcd_display.hpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief LCD Display (STM32)
 *
 * @date 2023-07-05
 * @version 0.0.1
 ***************************************************************************/

#pragma once

#include "fonts.hpp"
#include "return_code.hpp"

// from DISCO_H747I/Drivers/STM32H7xx_HAL_Driver
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_rcc_ex.h"

// from DISCO_H747I/Drivers/BSP/STM32H747I-DISCO
#include "stm32h747i_discovery_lcd.h"

// from DISCO_H747I/Utilities/Fonts

// from DISCO_H747I/Drivers/BSP/Components/Common
#include "lcd.h"

// from DISCO_H747I/Drivers/BSP/Components/otm8009a
#include "otm8009a.h"

namespace disco {

class LCDDisplay {
   public:
    LCDDisplay() = default;
    ReturnCode init();
    void fillDisplay(uint32_t color);
    void fillRectangle(
        uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height, uint32_t color);
    void setFont(Font* pFont);
    Font* getFont();
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getTitleHeight() const;
    enum class AlignMode {
        CENTER_MODE = 0x01, /*!< Center mode */
        RIGHT_MODE  = 0x02, /*!< Right mode  */
        LEFT_MODE   = 0x03  /*!< Left mode   */
    };
    void displayWelcome(const char* text, AlignMode alignMode);
    void displayTitle(const char* text, AlignMode alignMode);
    void displayPicture(
        const uint32_t* pSrc, uint16_t x, uint16_t y, uint16_t xsize, uint16_t ysize);
    void displayStringAtLine(uint32_t line, const char* text, AlignMode alignMode);
    void displayStringAt(uint32_t xPos, uint32_t yPos, const char* text, AlignMode mode);
    void displayVerticalLine(uint32_t xPos, uint32_t width);
    void displayHorizontalLine(uint32_t yPos, uint32_t width);
    void refreshLCD();

    // public constants
    static constexpr uint32_t LCD_COLOR_BLUE  = 0xFF0000FFUL;
    static constexpr uint32_t LCD_COLOR_WHITE = 0xFFFFFFFFUL;
    static constexpr uint32_t LCD_COLOR_BLACK = 0x00000000UL;

   private:
    // private methods
    void mspInit();
    void ltdcInit();

    // draw context related methods
    uint32_t computeDisplayLineNumber(uint32_t line);
    void setTextColor(uint32_t color);
    void setBackColor(uint32_t color);
    void fillRect(
        uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height, uint32_t color);
    void fillRGBRect(
        uint32_t xPos, uint32_t yPos, uint8_t* pData, uint32_t width, uint32_t height);
    void displayChar(uint32_t xPos, uint32_t yPos, uint8_t ascii);
    void drawChar(uint32_t xPos, uint32_t yPos, const uint8_t* pData);
    static int32_t getXSize(uint32_t instance, uint32_t* xSize);
    static int32_t getYSize(uint32_t instance, uint32_t* ySize);

    // helpers
    static int32_t DSI_IO_Write(uint16_t channelNbr,
                                uint16_t reg,
                                uint8_t* pData,
                                uint16_t size);
    static int32_t DSI_IO_Read(uint16_t channelNbr,
                               uint16_t reg,
                               uint8_t* pData,
                               uint16_t size);
    static void LCD_LayerInit(uint16_t layerIndex, uint32_t address);

    // data members
    OTM8009A_Object_t otm8009AObj_;
    OTM8009A_Object_t* lcd_CompObj_               = nullptr;
    RCC_PeriphCLKInitTypeDef periphClkInitStruct_ = {0};
    DSI_PLLInitTypeDef dsiPllInit_                = {0};
    DSI_CmdCfgTypeDef cmdCfg_                     = {0};
    DSI_LPCmdTypeDef lpCmd_                       = {0};
    DMA2D_HandleTypeDef hdma2d_                   = {0};

    // lcd related
    static constexpr uint8_t kMaxNbrOfLayers = 2;
    struct LCDContext {
        // cppcheck-suppress unusedStructMember
        uint32_t textColor; /*!< Specifies the color of text */
        // cppcheck-suppress unusedStructMember
        uint32_t backColor; /*!< Specifies the background color below the text */
        // cppcheck-suppress unusedStructMember
        Font* pFont; /*!< Specifies the font used for the text */
    };
    LCDContext drawProp_[kMaxNbrOfLayers] = {
        {.textColor = LCD_COLOR_BLUE, .backColor = LCD_COLOR_WHITE, .pFont = nullptr},
        {.textColor = LCD_COLOR_BLUE, .backColor = LCD_COLOR_WHITE, .pFont = nullptr}};
    uint32_t currentLCDLayer_   = 0;
    uint32_t lcdDevice_         = 0;
    uint32_t lcdXsize_          = 0;
    uint32_t lcdYsize_          = 0;
    uint32_t lcdPixelFormat_    = 0;
    LCD_UTILS_Drv_t funcDriver_ = {BSP_LCD_DrawBitmap,
                                   BSP_LCD_FillRGBRect,
                                   BSP_LCD_DrawHLine,
                                   BSP_LCD_DrawVLine,
                                   BSP_LCD_FillRect,
                                   BSP_LCD_ReadPixel,
                                   BSP_LCD_WritePixel,
                                   getXSize,
                                   getYSize,
                                   BSP_LCD_SetActiveLayer,
                                   BSP_LCD_GetPixelFormat};

    // constant definitions
    static constexpr uint32_t kDisplayWidth  = 800;
    static constexpr uint32_t kDisplayHeight = 480;
    static constexpr uint32_t kTitleHeight   = 112;

    static constexpr uint32_t LCD_FRAME_BUFFER = 0xD0000000;
    static constexpr uint32_t VSYNC            = 1;
    static constexpr uint32_t VBP              = 1;
    static constexpr uint32_t VFP              = 1;
    static constexpr uint32_t VACT             = 480;
    static constexpr uint32_t HSYNC            = 1;
    static constexpr uint32_t HBP              = 1;
    static constexpr uint32_t HFP              = 1;
};

}  // namespace disco
