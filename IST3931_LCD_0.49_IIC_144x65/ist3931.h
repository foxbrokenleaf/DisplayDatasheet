#ifndef __IST3931_H
#define __IST3931_H

#include <stdint.h>

/* 屏幕分辨率定义 */
#define IST3931_WIDTH           144
#define IST3931_HEIGHT          65
#define IST3931_PAGES           ((IST3931_HEIGHT + 7) / 8)  // 65/8 = 9页
#define IST3931_COL_ADDR_MAX    17  // 144/8 - 1 = 17

/* 字体大小定义 */
#define IST3931_8X16            8   // 8x16字体
#define IST3931_6X8             6   // 6x8字体

/* 填充模式定义 */
#define IST3931_UNFILLED        0   // 不填充
#define IST3931_FILLED          1   // 填充

/* I2C地址定义 - 修正：统一使用IST3931_ADDR */
#define IST3931_ADDR            0x7E        // 从机地址
#define IST3931_CMD_BYTE        0x80        // 命令字节标识
#define IST3931_DATA_BYTE       0xC0        // 数据字节标识

/* 函数声明 */

/* 初始化函数 */
void IST3931_Init(void);

/* 更新函数 */
void IST3931_Update(void);
void IST3931_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/* 显存控制函数 */
void IST3931_Clear(void);
void IST3931_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void IST3931_Reverse(void);
void IST3931_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/* 显示函数 */
void IST3931_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);
void IST3931_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);
void IST3931_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void IST3931_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void IST3931_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void IST3931_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void IST3931_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void IST3931_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t Image[][8]);
void IST3931_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);
void IST3931_Printf_New(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);

/* 绘图函数 */
void IST3931_DrawPoint(int16_t X, int16_t Y);
uint8_t IST3931_GetPoint(int16_t X, int16_t Y);
void IST3931_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void IST3931_DrawDashedLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void IST3931_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void IST3931_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void IST3931_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);

/* 硬件控制函数 */
void IST3931_DisplayOn(void);
void IST3931_DisplayOff(void);
void IST3931_SetContrast(uint8_t Contrast);
void IST3931_SetSleepMode(uint8_t Enable);
void IST3931_Refresh(void);

/* 底层I2C函数 - 供内部使用 */
void IST3931_WriteCommand(uint8_t Command);
void IST3931_WriteData(uint8_t Data);
void IST3931_WriteDataMulti(uint8_t *Data, uint16_t Count);
void IST3931_SetCursor(uint8_t Y, uint8_t X);
void IST3931_HardwareReset(void);
void ist3931_disp_clear(void);

/* 隔行扫描映射函数 */
uint8_t IST3931_MapCOM(uint8_t physical_y);
uint8_t IST3931_MapPhysicalY(uint8_t logical_y);

/* 延时函数 */
void IST3931_DelayUs(uint32_t us);
void IST3931_DelayMs(uint32_t ms);


void IST3931_Test_2(uint8_t FontIndex, uint8_t DisplayIndex);
void IST3931_Test(uint8_t FontIndex, uint8_t DisplayIndex);

#endif