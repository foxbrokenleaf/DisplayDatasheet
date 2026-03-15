#ifndef __EPD_H
#define __EPD_H

#include <stdint.h>
#include "EPD_Data.h"

/*屏幕方向定义*********************/
#define EPD_ORIENTATION_PORTRAIT        0   // 竖屏（默认）
#define EPD_ORIENTATION_LANDSCAPE       1   // 横屏（顺时针旋转90度）
#define EPD_ORIENTATION_PORTRAIT_180    2   // 竖屏旋转180度
#define EPD_ORIENTATION_LANDSCAPE_180   3   // 横屏旋转180度（逆时针旋转90度）

/*选择屏幕方向（用户可修改）*********************/
#define EPD_ORIENTATION                 EPD_ORIENTATION_LANDSCAPE

/*物理屏幕尺寸（固定值，与方向无关）*********************/
#define EPD_PHYSICAL_WIDTH              152
#define EPD_PHYSICAL_HEIGHT             296
#define EPD_PHYSICAL_COLUMN_BYTES       19      // 152/8

/*逻辑屏幕尺寸（根据方向自动调整，供应用层使用）*********************/
#if (EPD_ORIENTATION == EPD_ORIENTATION_PORTRAIT) || (EPD_ORIENTATION == EPD_ORIENTATION_PORTRAIT_180)
    #define EPD_WIDTH               152     // 竖屏逻辑宽度
    #define EPD_HEIGHT              296     // 竖屏逻辑高度
    #define EPD_COLUMN_BYTES        19      // 竖屏每行字节数
#else
    #define EPD_WIDTH               296     // 横屏逻辑宽度
    #define EPD_HEIGHT              152     // 横屏逻辑高度
    #define EPD_COLUMN_BYTES        37      // 横屏每行字节数 (296/8)
#endif

/*字体大小定义*********************/
#define EPD_8X16                8       // 宽8像素，高16像素
#define EPD_6X8                 6       // 宽6像素，高8像素

/*填充模式定义*********************/
#define EPD_UNFILLED            0
#define EPD_FILLED              1

/*命令定义*********************/
#define EPD_CMD_PSR                     0x00  // Panel Setting
#define EPD_CMD_PWR                     0x01  // Power Setting
#define EPD_CMD_PWR_SEQ                  0x03  // Power Sequence
#define EPD_CMD_BTST                     0x06  // Booster Soft Start
#define EPD_CMD_PLL                      0x30  // PLL Control
#define EPD_CMD_TSE                      0x41  // Temperature Sensor Enable
#define EPD_CMD_CDI                      0x50  // VCOM and Data Interval
#define EPD_CMD_WRITE_TEMPERATURE        0xE5  // Write Temperature
#define EPD_CMD_APPLY_TEMPERATURE        0xE0  // Apply Temperature
#define EPD_CMD_WRITE_IMAGE_RAM_BW       0x10  // Write Black-White Image RAM
#define EPD_CMD_WRITE_IMAGE_RAM_RW       0x13  // Write Red Image RAM
#define EPD_CMD_POWER_ON                 0x04  // Power On
#define EPD_CMD_POWER_OFF                0x02  // Power Off
#define EPD_CMD_DISPLAY_REFRESH          0x12  // Display Refresh
#define EPD_CMD_SOFT_RESET               0x00  // Soft Reset

/*函数声明*********************/
void EPD_Init(void);
void EPD_Update(void);
void EPD_UpdateArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height);
void EPD_Clear(void);
void EPD_ClearArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height);
void EPD_Reverse(void);
void EPD_ReverseArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height);
void EPD_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void EPD_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);
void EPD_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void EPD_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void EPD_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void EPD_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void EPD_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void EPD_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);
void EPD_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);
void EPD_DrawPoint(int16_t X, int16_t Y);
void EPD_ClearPoint(int16_t X, int16_t Y);
uint8_t EPD_GetPoint(int16_t X, int16_t Y);
void EPD_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void EPD_DrawDashedLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void EPD_DrawRectangle(int16_t X, int16_t Y, uint16_t Width, uint16_t Height, uint8_t IsFilled);
void EPD_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void EPD_DrawCircle(int16_t X, int16_t Y, uint16_t Radius, uint8_t IsFilled);
void EPD_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

/*底层函数*/
void EPD_WriteCommand(uint8_t Command);
void EPD_WriteData(uint8_t Data);
void EPD_BUSY_Wait(void);

/*调试函数*/
void EPD_TestPattern(void);  // 测试图案，用于验证屏幕是否正常工作

#endif