#include "EPD.h"
#include "EPD_Data.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include "stm32f10x.h"

/**
  * 数据存储格式：
  * 纵向8点，高位在下，先从左到右，再从上到下
  * 每一个Bit对应一个像素点
  */

/*全局变量*********************/
/* 物理显存 - 始终按物理尺寸定义 */
static uint8_t EPD_DisplayBuf[EPD_PHYSICAL_HEIGHT][EPD_PHYSICAL_COLUMN_BYTES];

/*引脚定义*********************/
#define EPD_BUSY_PORT   GPIOA
#define EPD_BUSY_PIN    GPIO_Pin_5
#define EPD_RST_PORT    GPIOA
#define EPD_RST_PIN     GPIO_Pin_4
#define EPD_DC_PORT     GPIOA
#define EPD_DC_PIN      GPIO_Pin_3
#define EPD_CS_PORT     GPIOA
#define EPD_CS_PIN      GPIO_Pin_2
#define EPD_SCL_PORT    GPIOA
#define EPD_SCL_PIN     GPIO_Pin_1
#define EPD_SDA_PORT    GPIOA
#define EPD_SDA_PIN     GPIO_Pin_0

/*引脚操作宏*********************/
#define EPD_BUSY_READ()      GPIO_ReadInputDataBit(EPD_BUSY_PORT, EPD_BUSY_PIN)
#define EPD_RST_HIGH()       GPIO_SetBits(EPD_RST_PORT, EPD_RST_PIN)
#define EPD_RST_LOW()        GPIO_ResetBits(EPD_RST_PORT, EPD_RST_PIN)
#define EPD_DC_HIGH()        GPIO_SetBits(EPD_DC_PORT, EPD_DC_PIN)
#define EPD_DC_LOW()         GPIO_ResetBits(EPD_DC_PORT, EPD_DC_PIN)
#define EPD_CS_HIGH()        GPIO_SetBits(EPD_CS_PORT, EPD_CS_PIN)
#define EPD_CS_LOW()         GPIO_ResetBits(EPD_CS_PORT, EPD_CS_PIN)
#define EPD_SCL_HIGH()       GPIO_SetBits(EPD_SCL_PORT, EPD_SCL_PIN)
#define EPD_SCL_LOW()        GPIO_ResetBits(EPD_SCL_PORT, EPD_SCL_PIN)
#define EPD_SDA_HIGH()       GPIO_SetBits(EPD_SDA_PORT, EPD_SDA_PIN)
#define EPD_SDA_LOW()        GPIO_ResetBits(EPD_SDA_PORT, EPD_SDA_PIN)

/*物理屏幕尺寸（固定值）*********************/
#define EPD_PHYSICAL_WIDTH      152
#define EPD_PHYSICAL_HEIGHT     296
#define EPD_PHYSICAL_COLUMN_BYTES 19

/*延时函数*********************/
static void delay_ms(uint32_t ms)
{
    uint32_t i;
    while(ms--)
    {
        for(i = 0; i < 8000; i++);
    }
}

static void delay_us(uint32_t us)
{
    uint32_t i;
    while(us--)
    {
        for(i = 0; i < 8; i++);
    }
}

/*工具函数*********************/
static uint32_t EPD_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while(Y--)
    {
        Result *= X;
    }
    return Result;
}

static uint8_t EPD_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
    int16_t i, j, c = 0;
    
    for (i = 0, j = nvert - 1; i < nvert; j = i++)
    {
        if (((verty[i] > testy) != (verty[j] > testy)) &&
            (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
        {
            c = !c;
        }
    }
    return c;
}

static uint8_t EPD_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
    int16_t PointAngle;
    PointAngle = atan2(Y, X) / 3.14 * 180;
    if (StartAngle < EndAngle)
    {
        if (PointAngle >= StartAngle && PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    else
    {
        if (PointAngle >= StartAngle || PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    return 0;
}

/**
  * 函    数：坐标转换（将逻辑坐标系坐标转换为物理显存坐标）
  * 修正：确保横屏时数据正确映射到整个屏幕
  */
static void EPD_LogicToPhys(int16_t logicX, int16_t logicY, int16_t *physX, int16_t *physY)
{
#if (EPD_ORIENTATION == EPD_ORIENTATION_PORTRAIT)
    /* 竖屏：坐标不变 */
    *physX = logicX;
    *physY = logicY;
    
#elif (EPD_ORIENTATION == EPD_ORIENTATION_PORTRAIT_180)
    /* 竖屏180度：旋转180度 */
    *physX = EPD_PHYSICAL_WIDTH - 1 - logicX;
    *physY = EPD_PHYSICAL_HEIGHT - 1 - logicY;
    
#elif (EPD_ORIENTATION == EPD_ORIENTATION_LANDSCAPE)
    /* 横屏（顺时针旋转90度）
       逻辑宽度 296，逻辑高度 152
       物理宽度 152，物理高度 296
       
       映射关系：
       逻辑X (0-295) 映射到 物理Y (0-295)
       逻辑Y (0-151) 映射到 物理X (0-151)
       并且需要将逻辑X的方向反转，因为屏幕扫描是从左到右
    */
    *physX = logicY;                          // 逻辑Y -> 物理X
    *physY = EPD_PHYSICAL_HEIGHT - 1 - logicX; // 逻辑X反转后 -> 物理Y
    
#elif (EPD_ORIENTATION == EPD_ORIENTATION_LANDSCAPE_180)
    /* 横屏180度（逆时针旋转90度） */
    *physX = EPD_PHYSICAL_WIDTH - 1 - logicY;  // 逻辑Y反转 -> 物理X
    *physY = logicX;                            // 逻辑X -> 物理Y
    
#else
    *physX = logicX;
    *physY = logicY;
#endif
}

/**
  * 函    数：检查点是否在逻辑坐标系范围内
  */
static uint8_t EPD_IsInLogicRange(int16_t X, int16_t Y)
{
    if(X >= 0 && X < EPD_WIDTH && Y >= 0 && Y < EPD_HEIGHT)
    {
        return 1;
    }
    return 0;
}

/**
  * 函    数：检查点是否在物理屏幕范围内
  */
static uint8_t EPD_IsInPhysicalRange(int16_t X, int16_t Y)
{
    if(X >= 0 && X < EPD_PHYSICAL_WIDTH && Y >= 0 && Y < EPD_PHYSICAL_HEIGHT)
    {
        return 1;
    }
    return 0;
}

/*引脚配置*********************/
void EPD_W_SCL(uint8_t BitValue)
{
    if(BitValue)
        EPD_SCL_HIGH();
    else
        EPD_SCL_LOW();
}

void EPD_W_SDA(uint8_t BitValue)
{
    if(BitValue)
        EPD_SDA_HIGH();
    else
        EPD_SDA_LOW();
}

void EPD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t i, j;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = EPD_BUSY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(EPD_BUSY_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = EPD_RST_PIN | EPD_DC_PIN | EPD_CS_PIN | 
                                  EPD_SCL_PIN | EPD_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(EPD_RST_PORT, &GPIO_InitStructure);
    
    for (i = 0; i < 1000; i ++)
    {
        for (j = 0; j < 1000; j ++);
    }
    
    EPD_RST_HIGH();
    EPD_DC_HIGH();
    EPD_CS_HIGH();
    EPD_SCL_HIGH();
    EPD_SDA_HIGH();
}

/*通信协议*********************/
void EPD_SPI_SendByte(uint8_t Byte)
{
    uint8_t i;
    
    for(i = 0; i < 8; i++)
    {
        EPD_W_SCL(0);
        EPD_W_SDA(!!(Byte & (0x80 >> i)));
        delay_us(1);
        EPD_W_SCL(1);
        delay_us(1);
        EPD_W_SCL(0);
    }
}

void EPD_WriteCommand(uint8_t Command)
{
    EPD_CS_LOW();
    EPD_DC_LOW();
    EPD_SPI_SendByte(Command);
    EPD_CS_HIGH();
}

void EPD_WriteData(uint8_t Data)
{
    EPD_CS_LOW();
    EPD_DC_HIGH();
    EPD_SPI_SendByte(Data);
    EPD_CS_HIGH();
}

void EPD_BUSY_Wait(void)
{
    while(EPD_BUSY_READ() != Bit_SET)
    {
        delay_ms(10);
    }
}

/*硬件配置*********************/
void EPD_COG_Reset(void)
{
    delay_ms(5);
    EPD_RST_HIGH();
    delay_ms(5);
    EPD_RST_LOW();
    delay_ms(10);
    EPD_RST_HIGH();
    delay_ms(5);
    EPD_CS_HIGH();
    delay_ms(5);
}

/**
  * 2.66inch屏幕初始化序列
  */
void EPD_Init(void)
{
    EPD_GPIO_Init();
    EPD_COG_Reset();
    
    /* 软复位 */
    EPD_WriteCommand(EPD_CMD_SOFT_RESET);
    EPD_WriteData(0xE0);
    delay_ms(5);
    
    /* Power Setting */
    EPD_WriteCommand(EPD_CMD_PWR);
    EPD_WriteData(0x3F);
    EPD_WriteData(0x00);
    EPD_WriteData(0x32);
    EPD_WriteData(0x2A);
    EPD_WriteData(0x0E);
    EPD_WriteData(0x2A);
    
    /* Booster Soft Start */
    EPD_WriteCommand(EPD_CMD_BTST);
    EPD_WriteData(0x17);
    EPD_WriteData(0x17);
    EPD_WriteData(0x17);
    
    /* Power Sequence */
    EPD_WriteCommand(EPD_CMD_PWR_SEQ);
    EPD_WriteData(0x41);
    EPD_WriteData(0x00);
    EPD_WriteData(0x32);
    
    /* PLL Control */
    EPD_WriteCommand(EPD_CMD_PLL);
    EPD_WriteData(0x3C);
    
    /* Temperature Sensor Enable */
    EPD_WriteCommand(EPD_CMD_TSE);
    EPD_WriteData(0x00);
    
    /* Panel Setting */
    EPD_WriteCommand(EPD_CMD_PSR);
    EPD_WriteData(0xCF);  // 152x296, LUT from OTP
    EPD_WriteData(0x8D);  // 使用默认LUT
    
    /* VCOM and Data Interval */
    EPD_WriteCommand(EPD_CMD_CDI);
    EPD_WriteData(0x97);
    
    /* 设置温度 */
    EPD_WriteCommand(EPD_CMD_WRITE_TEMPERATURE);
    EPD_WriteData(0x19);  // 25°C
    
    EPD_WriteCommand(EPD_CMD_APPLY_TEMPERATURE);
    EPD_WriteData(0x02);
    
    EPD_Clear();
}

/**
  * 函    数：测试图案 - 用于验证屏幕是否正常工作
  */
void EPD_TestPattern(void)
{
    uint16_t i;
    
    /* 清屏 */
    EPD_Clear();
    
    /* 绘制完整的边框 */
    for(i = 0; i < EPD_WIDTH; i++)
    {
        EPD_DrawPoint(i, 0);                 // 上边框
        EPD_DrawPoint(i, EPD_HEIGHT - 1);    // 下边框
    }
    for(i = 0; i < EPD_HEIGHT; i++)
    {
        EPD_DrawPoint(0, i);                  // 左边框
        EPD_DrawPoint(EPD_WIDTH - 1, i);      // 右边框
    }
    
    /* 绘制对角线 */
    for(i = 0; i < EPD_WIDTH && i < EPD_HEIGHT; i++)
    {
        EPD_DrawPoint(i, i);                          // 左上到右下
        EPD_DrawPoint(EPD_WIDTH - 1 - i, i);          // 右上到左下
    }
    
    /* 绘制中心十字线 */
    for(i = 0; i < EPD_WIDTH; i++)
    {
        EPD_DrawPoint(i, EPD_HEIGHT/2);        // 水平中线
    }
    for(i = 0; i < EPD_HEIGHT; i++)
    {
        EPD_DrawPoint(EPD_WIDTH/2, i);         // 垂直中线
    }
    
    /* 在四个角显示文字 */
    EPD_ShowString(10, 10, "TL", EPD_8X16);                    // 左上
    EPD_ShowString(EPD_WIDTH - 40, 10, "TR", EPD_8X16);       // 右上
    EPD_ShowString(10, EPD_HEIGHT - 30, "BL", EPD_8X16);      // 左下
    EPD_ShowString(EPD_WIDTH - 40, EPD_HEIGHT - 30, "BR", EPD_8X16); // 右下
    
    /* 在中心显示文字 */
    EPD_ShowString(EPD_WIDTH/2 - 40, EPD_HEIGHT/2 - 8, "TEST", EPD_8X16);
    
    EPD_Update();
}

/*功能函数*********************/
/**
  * 函    数：将EPD显存数组更新到EPD屏幕
  */
void EPD_Update(void)
{
    uint16_t i, j;
    
    /* 写入黑白图像数据 */
    EPD_WriteCommand(EPD_CMD_WRITE_IMAGE_RAM_BW);
    for(i = 0; i < EPD_PHYSICAL_HEIGHT; i++)
    {
        for(j = 0; j < EPD_PHYSICAL_COLUMN_BYTES; j++)
        {
            EPD_WriteData(EPD_DisplayBuf[i][j]);
        }
    }
    
    /* 红色通道全0 */
    EPD_WriteCommand(EPD_CMD_WRITE_IMAGE_RAM_RW);
    for(i = 0; i < EPD_PHYSICAL_HEIGHT; i++)
    {
        for(j = 0; j < EPD_PHYSICAL_COLUMN_BYTES; j++)
        {
            EPD_WriteData(0x00);
        }
    }
    
    /* 刷新显示 */
    EPD_WriteCommand(EPD_CMD_POWER_ON);
    delay_ms(5);
    EPD_BUSY_Wait();
    
    EPD_WriteCommand(EPD_CMD_DISPLAY_REFRESH);
    delay_ms(5);
    EPD_BUSY_Wait();
    
    EPD_WriteCommand(EPD_CMD_POWER_OFF);
    EPD_BUSY_Wait();
}

void EPD_UpdateArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height)
{
    /* 简化处理，直接更新全屏 */
    EPD_Update();
}

/**
  * 函    数：将EPD显存数组全部清零（设置为白色）
  */
void EPD_Clear(void)
{
    uint16_t i, j;
    for(i = 0; i < EPD_PHYSICAL_HEIGHT; i++)
    {
        for(j = 0; j < EPD_PHYSICAL_COLUMN_BYTES; j++)
        {
            EPD_DisplayBuf[i][j] = 0xFF;
        }
    }
}

/**
  * 函    数：将EPD显存数组部分清零（设置为白色）
  */
void EPD_ClearArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height)
{
    int16_t i, j;
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    for(j = Y; j < Y + Height; j++)
    {
        for(i = X; i < X + Width; i++)
        {
            if(EPD_IsInLogicRange(i, j))
            {
                EPD_LogicToPhys(i, j, &physX, &physY);
                
                if(EPD_IsInPhysicalRange(physX, physY))
                {
                    byteIndex = physX / 8;
                    bitIndex = 7 - (physX % 8);
                    EPD_DisplayBuf[physY][byteIndex] |= (0x01 << bitIndex);
                }
            }
        }
    }
}

/**
  * 函    数：将EPD显存数组全部取反
  */
void EPD_Reverse(void)
{
    uint16_t i, j;
    for(i = 0; i < EPD_PHYSICAL_HEIGHT; i++)
    {
        for(j = 0; j < EPD_PHYSICAL_COLUMN_BYTES; j++)
        {
            EPD_DisplayBuf[i][j] ^= 0xFF;
        }
    }
}

/**
  * 函    数：将EPD显存数组部分取反
  */
void EPD_ReverseArea(int16_t X, int16_t Y, uint16_t Width, uint16_t Height)
{
    int16_t i, j;
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    for(j = Y; j < Y + Height; j++)
    {
        for(i = X; i < X + Width; i++)
        {
            if(EPD_IsInLogicRange(i, j))
            {
                EPD_LogicToPhys(i, j, &physX, &physY);
                
                if(EPD_IsInPhysicalRange(physX, physY))
                {
                    byteIndex = physX / 8;
                    bitIndex = 7 - (physX % 8);
                    EPD_DisplayBuf[physY][byteIndex] ^= (0x01 << bitIndex);
                }
            }
        }
    }
}

/**
  * 函    数：EPD显示一个字符
  */
void EPD_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
    uint8_t i, j;
    uint8_t temp;
    uint8_t pos;
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    if(FontSize == EPD_8X16)
    {
        pos = Char - ' ';
        
        for(i = 0; i < 16; i++)
        {
            temp = EPD_F8x16[pos][i];
            
            for(j = 0; j < 8; j++)
            {
                if(EPD_IsInLogicRange(X + j, Y + i))
                {
                    EPD_LogicToPhys(X + j, Y + i, &physX, &physY);
                    
                    if(EPD_IsInPhysicalRange(physX, physY))
                    {
                        byteIndex = physX / 8;
                        bitIndex = 7 - (physX % 8);
                        
                        if(temp & (0x80 >> j))
                        {
                            EPD_DisplayBuf[physY][byteIndex] &= ~(0x01 << bitIndex);
                        }
                        else
                        {
                            EPD_DisplayBuf[physY][byteIndex] |= (0x01 << bitIndex);
                        }
                    }
                }
            }
        }
    }
    else if(FontSize == EPD_6X8)
    {
        pos = Char - ' ';
        
        for(j = 0; j < 6; j++)
        {
            temp = EPD_F6x8[pos][j];
            
            for(i = 0; i < 8; i++)
            {
                if(EPD_IsInLogicRange(X + j, Y + i))
                {
                    EPD_LogicToPhys(X + j, Y + i, &physX, &physY);
                    
                    if(EPD_IsInPhysicalRange(physX, physY))
                    {
                        byteIndex = physX / 8;
                        bitIndex = 7 - (physX % 8);
                        
                        if(temp & (1 << i))
                        {
                            EPD_DisplayBuf[physY][byteIndex] &= ~(0x01 << bitIndex);
                        }
                        else
                        {
                            EPD_DisplayBuf[physY][byteIndex] |= (0x01 << bitIndex);
                        }
                    }
                }
            }
        }
    }
}

void EPD_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize)
{
    uint16_t XOffset = 0;
    
    while(*String != '\0')
    {
        EPD_ShowChar(X + XOffset, Y, *String, FontSize);
        XOffset += FontSize;
        String++;
    }
}

void EPD_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    for(i = 0; i < Length; i++)
    {
        EPD_ShowChar(X + i * FontSize, Y, 
                     Number / EPD_Pow(10, Length - i - 1) % 10 + '0', FontSize);
    }
}

void EPD_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint32_t Number1;
    
    if(Number >= 0)
    {
        EPD_ShowChar(X, Y, '+', FontSize);
        Number1 = Number;
    }
    else
    {
        EPD_ShowChar(X, Y, '-', FontSize);
        Number1 = -Number;
    }
    
    for(i = 0; i < Length; i++)
    {
        EPD_ShowChar(X + (i + 1) * FontSize, Y, 
                     Number1 / EPD_Pow(10, Length - i - 1) % 10 + '0', FontSize);
    }
}

void EPD_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i, SingleNumber;
    for(i = 0; i < Length; i++)
    {
        SingleNumber = Number / EPD_Pow(16, Length - i - 1) % 16;
        
        if(SingleNumber < 10)
        {
            EPD_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontSize);
        }
        else
        {
            EPD_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontSize);
        }
    }
}

void EPD_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    for(i = 0; i < Length; i++)
    {
        EPD_ShowChar(X + i * FontSize, Y, 
                     Number / EPD_Pow(2, Length - i - 1) % 2 + '0', FontSize);
    }
}

void EPD_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
    uint32_t PowNum, IntNum, FraNum;
    
    if(Number >= 0)
    {
        EPD_ShowChar(X, Y, '+', FontSize);
    }
    else
    {
        EPD_ShowChar(X, Y, '-', FontSize);
        Number = -Number;
    }
    
    IntNum = Number;
    Number -= IntNum;
    PowNum = EPD_Pow(10, FraLength);
    FraNum = round(Number * PowNum);
    IntNum += FraNum / PowNum;
    
    EPD_ShowNum(X + FontSize, Y, IntNum, IntLength, FontSize);
    EPD_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontSize);
    EPD_ShowNum(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength, FontSize);
}

/**
  * 函    数：EPD显示图像
  */
void EPD_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
    uint16_t i, j;
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    uint8_t pixel;
    uint16_t bytesPerLine;
    
    bytesPerLine = (Width + 7) / 8;
    
    /* 先清空显示区域 */
    for(j = 0; j < Height; j++)
    {
        for(i = 0; i < Width; i++)
        {
            if(EPD_IsInLogicRange(X + i, Y + j))
            {
                EPD_LogicToPhys(X + i, Y + j, &physX, &physY);
                
                if(EPD_IsInPhysicalRange(physX, physY))
                {
                    byteIndex = physX / 8;
                    bitIndex = 7 - (physX % 8);
                    EPD_DisplayBuf[physY][byteIndex] |= (0x01 << bitIndex);
                }
            }
        }
    }
    
    /* 绘制图像 */
    for(j = 0; j < Height; j++)
    {
        for(i = 0; i < Width; i++)
        {
            if(EPD_IsInLogicRange(X + i, Y + j))
            {
                EPD_LogicToPhys(X + i, Y + j, &physX, &physY);
                
                if(EPD_IsInPhysicalRange(physX, physY))
                {
                    byteIndex = physX / 8;
                    bitIndex = 7 - (physX % 8);
                    
                    pixel = (Image[j * bytesPerLine + i / 8] >> (7 - (i % 8))) & 0x01;
                    
                    if(pixel)
                    {
                        EPD_DisplayBuf[physY][byteIndex] &= ~(0x01 << bitIndex);
                    }
                }
            }
        }
    }
}

void EPD_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{
    char String[256];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    EPD_ShowString(X, Y, String, FontSize);
}

/**
  * 函    数：EPD在指定位置画一个点（黑色）
  */
void EPD_DrawPoint(int16_t X, int16_t Y)
{
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    if(EPD_IsInLogicRange(X, Y))
    {
        EPD_LogicToPhys(X, Y, &physX, &physY);
        
        if(EPD_IsInPhysicalRange(physX, physY))
        {
            byteIndex = physX / 8;
            bitIndex = 7 - (physX % 8);
            EPD_DisplayBuf[physY][byteIndex] &= ~(0x01 << bitIndex);
        }
    }
}

/**
  * 函    数：EPD擦除指定位置的点（设置为白色）
  */
void EPD_ClearPoint(int16_t X, int16_t Y)
{
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    if(EPD_IsInLogicRange(X, Y))
    {
        EPD_LogicToPhys(X, Y, &physX, &physY);
        
        if(EPD_IsInPhysicalRange(physX, physY))
        {
            byteIndex = physX / 8;
            bitIndex = 7 - (physX % 8);
            EPD_DisplayBuf[physY][byteIndex] |= (0x01 << bitIndex);
        }
    }
}

uint8_t EPD_GetPoint(int16_t X, int16_t Y)
{
    int16_t physX, physY;
    uint8_t byteIndex, bitIndex;
    
    if(EPD_IsInLogicRange(X, Y))
    {
        EPD_LogicToPhys(X, Y, &physX, &physY);
        
        if(EPD_IsInPhysicalRange(physX, physY))
        {
            byteIndex = physX / 8;
            bitIndex = 7 - (physX % 8);
            if(EPD_DisplayBuf[physY][byteIndex] & (0x01 << bitIndex))
            {
                return 1;
            }
        }
    }
    return 0;
}

/* 以下绘图函数保持不变 */
void EPD_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;
    
    if(y0 == y1)
    {
        if(x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
        for(x = x0; x <= x1; x++)
        {
            EPD_DrawPoint(x, y0);
        }
    }
    else if(x0 == x1)
    {
        if(y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
        for(y = y0; y <= y1; y++)
        {
            EPD_DrawPoint(x0, y);
        }
    }
    else
    {
        if(x0 > x1)
        {
            temp = x0; x0 = x1; x1 = temp;
            temp = y0; y0 = y1; y1 = temp;
        }
        
        if(y0 > y1)
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }
        
        if(y1 - y0 > x1 - x0)
        {
            temp = x0; x0 = y0; y0 = temp;
            temp = x1; x1 = y1; y1 = temp;
            xyflag = 1;
        }
        
        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;
        
        if(yflag && xyflag){EPD_DrawPoint(y, -x);}
        else if(yflag)     {EPD_DrawPoint(x, -y);}
        else if(xyflag)    {EPD_DrawPoint(y, x);}
        else               {EPD_DrawPoint(x, y);}
        
        while(x < x1)
        {
            x++;
            if(d < 0)
            {
                d += incrE;
            }
            else
            {
                y++;
                d += incrNE;
            }
            
            if(yflag && xyflag){EPD_DrawPoint(y, -x);}
            else if(yflag)     {EPD_DrawPoint(x, -y);}
            else if(xyflag)    {EPD_DrawPoint(y, x);}
            else               {EPD_DrawPoint(x, y);}
        }
    }
}

void EPD_DrawDashedLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
    const uint8_t dashLength = 3;
    const uint8_t gapLength = 2;
    
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;
    uint16_t pointCount = 0;
    
    if(y0 == y1)
    {
        if(x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
        
        for(x = x0; x <= x1; x++)
        {
            if(pointCount < dashLength)
            {
                EPD_DrawPoint(x, y0);
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
    else if(x0 == x1)
    {
        if(y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
        
        for(y = y0; y <= y1; y++)
        {
            if(pointCount < dashLength)
            {
                EPD_DrawPoint(x0, y);
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
    else
    {
        if(x0 > x1)
        {
            temp = x0; x0 = x1; x1 = temp;
            temp = y0; y0 = y1; y1 = temp;
        }
        
        if(y0 > y1)
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }
        
        if(y1 - y0 > x1 - x0)
        {
            temp = x0; x0 = y0; y0 = temp;
            temp = x1; x1 = y1; y1 = temp;
            xyflag = 1;
        }
        
        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;
        
        if(pointCount < dashLength)
        {
            if(yflag && xyflag){EPD_DrawPoint(y, -x);}
            else if(yflag)     {EPD_DrawPoint(x, -y);}
            else if(xyflag)    {EPD_DrawPoint(y, x);}
            else               {EPD_DrawPoint(x, y);}
        }
        pointCount = (pointCount + 1) % (dashLength + gapLength);
        
        while(x < x1)
        {
            x++;
            if(d < 0)
            {
                d += incrE;
            }
            else
            {
                y++;
                d += incrNE;
            }
            
            if(pointCount < dashLength)
            {
                if(yflag && xyflag){EPD_DrawPoint(y, -x);}
                else if(yflag)     {EPD_DrawPoint(x, -y);}
                else if(xyflag)    {EPD_DrawPoint(y, x);}
                else               {EPD_DrawPoint(x, y);}
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
}

void EPD_DrawRectangle(int16_t X, int16_t Y, uint16_t Width, uint16_t Height, uint8_t IsFilled)
{
    int16_t i, j;
    
    if(!IsFilled)
    {
        for(i = X; i < X + Width; i++)
        {
            EPD_DrawPoint(i, Y);
            EPD_DrawPoint(i, Y + Height - 1);
        }
        for(i = Y; i < Y + Height; i++)
        {
            EPD_DrawPoint(X, i);
            EPD_DrawPoint(X + Width - 1, i);
        }
    }
    else
    {
        for(i = X; i < X + Width; i++)
        {
            for(j = Y; j < Y + Height; j++)
            {
                EPD_DrawPoint(i, j);
            }
        }
    }
}

void EPD_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, 
                      int16_t X2, int16_t Y2, uint8_t IsFilled)
{
    int16_t minx = X0, miny = Y0, maxx = X0, maxy = Y0;
    int16_t i, j;
    int16_t vx[] = {X0, X1, X2};
    int16_t vy[] = {Y0, Y1, Y2};
    
    if(!IsFilled)
    {
        EPD_DrawLine(X0, Y0, X1, Y1);
        EPD_DrawLine(X0, Y0, X2, Y2);
        EPD_DrawLine(X1, Y1, X2, Y2);
    }
    else
    {
        if(X1 < minx) {minx = X1;}
        if(X2 < minx) {minx = X2;}
        if(Y1 < miny) {miny = Y1;}
        if(Y2 < miny) {miny = Y2;}
        
        if(X1 > maxx) {maxx = X1;}
        if(X2 > maxx) {maxx = X2;}
        if(Y1 > maxy) {maxy = Y1;}
        if(Y2 > maxy) {maxy = Y2;}
        
        for(i = minx; i <= maxx; i++)
        {
            for(j = miny; j <= maxy; j++)
            {
                if(EPD_pnpoly(3, vx, vy, i, j))
                {
                    EPD_DrawPoint(i, j);
                }
            }
        }
    }
}

void EPD_DrawCircle(int16_t X, int16_t Y, uint16_t Radius, uint8_t IsFilled)
{
    int16_t x, y, d, j;
    
    d = 1 - Radius;
    x = 0;
    y = Radius;
    
    EPD_DrawPoint(X + x, Y + y);
    EPD_DrawPoint(X - x, Y - y);
    EPD_DrawPoint(X + y, Y + x);
    EPD_DrawPoint(X - y, Y - x);
    
    if(IsFilled)
    {
        for(j = -y; j < y; j++)
        {
            EPD_DrawPoint(X, Y + j);
        }
    }
    
    while(x < y)
    {
        x++;
        if(d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
        
        EPD_DrawPoint(X + x, Y + y);
        EPD_DrawPoint(X + y, Y + x);
        EPD_DrawPoint(X - x, Y - y);
        EPD_DrawPoint(X - y, Y - x);
        EPD_DrawPoint(X + x, Y - y);
        EPD_DrawPoint(X + y, Y - x);
        EPD_DrawPoint(X - x, Y + y);
        EPD_DrawPoint(X - y, Y + x);
        
        if(IsFilled)
        {
            for(j = -y; j < y; j++)
            {
                EPD_DrawPoint(X + x, Y + j);
                EPD_DrawPoint(X - x, Y + j);
            }
            for(j = -x; j < x; j++)
            {
                EPD_DrawPoint(X - y, Y + j);
                EPD_DrawPoint(X + y, Y + j);
            }
        }
    }
}

void EPD_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
    int16_t x, y, d, j;
    
    d = 1 - Radius;
    x = 0;
    y = Radius;
    
    if(EPD_IsInAngle(x, y, StartAngle, EndAngle)) {EPD_DrawPoint(X + x, Y + y);}
    if(EPD_IsInAngle(-x, -y, StartAngle, EndAngle)) {EPD_DrawPoint(X - x, Y - y);}
    if(EPD_IsInAngle(y, x, StartAngle, EndAngle)) {EPD_DrawPoint(X + y, Y + x);}
    if(EPD_IsInAngle(-y, -x, StartAngle, EndAngle)) {EPD_DrawPoint(X - y, Y - x);}
    
    if(IsFilled)
    {
        for(j = -y; j < y; j++)
        {
            if(EPD_IsInAngle(0, j, StartAngle, EndAngle)) {EPD_DrawPoint(X, Y + j);}
        }
    }
    
    while(x < y)
    {
        x++;
        if(d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
        
        if(EPD_IsInAngle(x, y, StartAngle, EndAngle)) {EPD_DrawPoint(X + x, Y + y);}
        if(EPD_IsInAngle(y, x, StartAngle, EndAngle)) {EPD_DrawPoint(X + y, Y + x);}
        if(EPD_IsInAngle(-x, -y, StartAngle, EndAngle)) {EPD_DrawPoint(X - x, Y - y);}
        if(EPD_IsInAngle(-y, -x, StartAngle, EndAngle)) {EPD_DrawPoint(X - y, Y - x);}
        if(EPD_IsInAngle(x, -y, StartAngle, EndAngle)) {EPD_DrawPoint(X + x, Y - y);}
        if(EPD_IsInAngle(y, -x, StartAngle, EndAngle)) {EPD_DrawPoint(X + y, Y - x);}
        if(EPD_IsInAngle(-x, y, StartAngle, EndAngle)) {EPD_DrawPoint(X - x, Y + y);}
        if(EPD_IsInAngle(-y, x, StartAngle, EndAngle)) {EPD_DrawPoint(X - y, Y + x);}
        
        if(IsFilled)
        {
            for(j = -y; j < y; j++)
            {
                if(EPD_IsInAngle(x, j, StartAngle, EndAngle)) {EPD_DrawPoint(X + x, Y + j);}
                if(EPD_IsInAngle(-x, j, StartAngle, EndAngle)) {EPD_DrawPoint(X - x, Y + j);}
            }
            for(j = -x; j < x; j++)
            {
                if(EPD_IsInAngle(-y, j, StartAngle, EndAngle)) {EPD_DrawPoint(X - y, Y + j);}
                if(EPD_IsInAngle(y, j, StartAngle, EndAngle)) {EPD_DrawPoint(X + y, Y + j);}
            }
        }
    }
}