/***************************************************************************************
  * 程序名称：				0.69寸OLED显示屏驱动程序（4针脚I2C接口）
  * 屏幕分辨率：				96x16
  * 驱动芯片：				SSD1306
  * 程序创建时间：			2025.03.16
  ***************************************************************************************
  */

#include "stm32f10x.h"
#include "OLED.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/**
  * 数据存储格式：
  * 纵向8点，高位在下，先从左到右，再从上到下
  * 每一个Bit对应一个像素点
  * 
  * 坐标轴定义：
  * 左上角为(0, 0)点
  * 横向向右为X轴，取值范围：0~95
  * 纵向向下为Y轴，取值范围：0~15
  * 
  *       0             X轴           95 
  *      .--------------------------->
  *    0 |
  *      |
  *  Y轴 |
  *      |
  *   15 |
  *      v
  * 
  */

/*全局变量*********************/

/**
  * OLED显存数组
  * 所有的显示函数，都只是对此显存数组进行读写
  * 随后调用OLED_Update函数或OLED_UpdateArea函数
  * 才会将显存数组的数据发送到OLED硬件，进行显示
  * 
  * 96x16分辨率：
  * Y方向16像素需要2页（每页8像素）
  * X方向96列
  */
uint8_t OLED_DisplayBuf[2][96];  // [页][列]，2页 * 96列

#define OLED_ADDR 0X78        
// #define OLED_ADDR 0X7A                                                                 
#define OLED_SCL_PORT GPIOB
#define OLED_SCL_PIN GPIO_Pin_6
#define OLED_SDA_PORT GPIOB
#define OLED_SDA_PIN GPIO_Pin_7

/*********************全局变量*/

/*引脚配置*********************/

/**
  * 函    数：OLED写SCL高低电平
  * 参    数：要写入SCL的电平值，范围：0/1
  * 返 回 值：无
  */
void OLED_W_SCL(uint8_t BitValue)
{
    GPIO_WriteBit(OLED_SCL_PORT, OLED_SCL_PIN, (BitAction)BitValue);
}

/**
  * 函    数：OLED写SDA高低电平
  * 参    数：要写入SDA的电平值，范围：0/1
  * 返 回 值：无
  */
void OLED_W_SDA(uint8_t BitValue)
{
    GPIO_WriteBit(OLED_SDA_PORT, OLED_SDA_PIN, (BitAction)BitValue);
}

/**
  * 函    数：OLED引脚初始化
  * 参    数：无
  * 返 回 值：无
  */
void OLED_GPIO_Init(void)
{
    uint32_t i, j;
    GPIO_InitTypeDef tmp;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    tmp.GPIO_Mode = GPIO_Mode_Out_PP;
    tmp.GPIO_Pin = OLED_SCL_PIN;
    tmp.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &tmp);
    
    tmp.GPIO_Mode = GPIO_Mode_Out_PP;
    tmp.GPIO_Pin = OLED_SDA_PIN;
    tmp.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &tmp); 
    
    /*在初始化前，加入适量延时，待OLED供电稳定*/
    for (i = 0; i < 1000; i ++)
    {
        for (j = 0; j < 1000; j ++);
    }
    
    /*释放SCL和SDA*/
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

/*********************引脚配置*/

/*通信协议*********************/

/**
  * 函    数：I2C起始
  * 参    数：无
  * 返 回 值：无
  */
void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_W_SDA(0);
    OLED_W_SCL(0);
}

/**
  * 函    数：I2C终止
  * 参    数：无
  * 返 回 值：无
  */
void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

/**
  * 函    数：I2C发送一个字节
  * 参    数：Byte 要发送的一个字节数据，范围：0x00~0xFF
  * 返 回 值：无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_W_SCL(1);
        OLED_W_SCL(0);
    }
    
    OLED_W_SCL(1);
    OLED_W_SCL(0);
}

/**
  * 函    数：OLED写命令
  * 参    数：Command 要写入的命令值，范围：0x00~0xFF
  * 返 回 值：无
  */
void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(OLED_ADDR);
    OLED_I2C_SendByte(0x00);
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

/**
  * 函    数：OLED写数据
  * 参    数：Data 要写入数据的起始地址
  * 参    数：Count 要写入数据的数量
  * 返 回 值：无
  */
void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
    uint8_t i;
    
    OLED_I2C_Start();
    OLED_I2C_SendByte(OLED_ADDR);
    OLED_I2C_SendByte(0x40);
    for (i = 0; i < Count; i ++)
    {
        OLED_I2C_SendByte(Data[i]);
    }
    OLED_I2C_Stop();
}

/*********************通信协议*/

/*硬件配置*********************/

/**
  * 函    数：OLED初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用前，需要调用此初始化函数
  */
void OLED_Init(void)
{
    OLED_GPIO_Init();
    
    /* SSD1306初始化序列（适用于96x16）*/
    OLED_WriteCommand(0xAE);    // 关闭显示
    
    OLED_WriteCommand(0xD5);    // 设置显示时钟分频比/振荡器频率
    OLED_WriteCommand(0x80);    // 建议值
    
    OLED_WriteCommand(0xA8);    // 设置多路复用率
    OLED_WriteCommand(0x0F);    // 96x16: 16行 (0x0F = 15, 行数为15+1=16)
    
    OLED_WriteCommand(0xD3);    // 设置显示偏移
    OLED_WriteCommand(0x00);    // 无偏移
    
    OLED_WriteCommand(0x40);    // 设置显示开始行 (0x40)
    
    OLED_WriteCommand(0x8D);    // 设置充电泵
    OLED_WriteCommand(0x14);    // 开启充电泵
    
    OLED_WriteCommand(0x20);    // 设置内存寻址模式
    OLED_WriteCommand(0x00);    // 水平寻址模式
    
    OLED_WriteCommand(0xA1);    // 设置左右方向 (A1正常, A0左右反置)
    
    OLED_WriteCommand(0xC8);    // 设置上下方向 (C8正常, C0上下反置)
    
    OLED_WriteCommand(0xDA);    // 设置COM引脚硬件配置
    OLED_WriteCommand(0x02);    // 96x16: 使用0x02 (16行)
    
    OLED_WriteCommand(0x81);    // 设置对比度
    OLED_WriteCommand(0xCF);    // 对比度值
    
    OLED_WriteCommand(0xD9);    // 设置预充电周期
    OLED_WriteCommand(0xF1);    // 建议值
    
    OLED_WriteCommand(0xDB);    // 设置VCOMH取消选择级别
    OLED_WriteCommand(0x40);    // 建议值
    
    OLED_WriteCommand(0xA4);    // 设置整个显示打开/关闭 (A4正常显示)
    
    OLED_WriteCommand(0xA6);    // 设置正常/反色显示 (A6正常)
    
    OLED_WriteCommand(0xAF);    // 开启显示
    
    OLED_Clear();               // 清空显存数组
    OLED_Update();              // 更新显示
}

/**
  * 函    数：OLED设置显示光标位置
  * 参    数：Page 指定光标所在的页，范围：0~1 (因为16像素 = 2页)
  * 参    数：X 指定光标所在的X轴坐标，范围：0~95
  * 返 回 值：无
  */
void OLED_SetCursor(uint8_t Page, uint8_t X)
{
    /* 96x16分辨率：只有0-1页，X坐标0-95 */
    if (Page > 1) Page = 1;
    if (X > 95) X = 95;
    
    /* 通过指令设置页地址和列地址 */
    OLED_WriteCommand(0xB0 | Page);                 // 设置页位置 (0xB0-0xB7)
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    // 设置X位置高4位
    OLED_WriteCommand(0x00 | (X & 0x0F));           // 设置X位置低4位
}

/*********************硬件配置*/

/*工具函数*********************/

/**
  * 函    数：次方函数
  * 参    数：X 底数
  * 参    数：Y 指数
  * 返 回 值：等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y --)
    {
        Result *= X;
    }
    return Result;
}

/*********************工具函数*/

/*功能函数*********************/

/**
  * 函    数：将OLED显存数组更新到OLED屏幕
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Update(void)
{
    uint8_t j;
    /* 遍历每一页 (2页) */
    for (j = 0; j < 2; j ++)
    {
        /* 设置光标位置为每一页的第一列 */
        OLED_SetCursor(j, 0);
        /* 连续写入96个数据，将显存数组的数据写入到OLED硬件 */
        OLED_WriteData(OLED_DisplayBuf[j], 96);
    }
}

/**
  * 函    数：将OLED显存数组部分更新到OLED屏幕
  * 参    数：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Width 指定区域的宽度，范围：0~96
  * 参    数：Height 指定区域的高度，范围：0~16
  * 返 回 值：无
  */
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t j;
    int16_t Page, Page1;
    
    Page = Y / 8;
    Page1 = (Y + Height - 1) / 8 + 1;
    if (Y < 0)
    {
        Page -= 1;
        Page1 -= 1;
    }
    
    /* 遍历指定区域涉及的相关页 */
    for (j = Page; j < Page1; j ++)
    {
        if (X >= 0 && X <= 95 && j >= 0 && j <= 1)
        {
            OLED_SetCursor(j, X);
            OLED_WriteData(&OLED_DisplayBuf[j][X], Width);
        }
    }
}

/**
  * 函    数：将OLED显存数组全部清零
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 2; j ++)            // 遍历2页
    {
        for (i = 0; i < 96; i ++)       // 遍历96列
        {
            OLED_DisplayBuf[j][i] = 0x00;
        }
    }
}

/**
  * 函    数：将OLED显存数组部分清零
  * 参    数：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Width 指定区域的宽度，范围：0~96
  * 参    数：Height 指定区域的高度，范围：0~16
  * 返 回 值：无
  */
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t i, j;
    
    for (j = Y; j < Y + Height; j ++)
    {
        for (i = X; i < X + Width; i ++)
        {
            if (i >= 0 && i <= 95 && j >= 0 && j <= 15)
            {
                OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
            }
        }
    }
}

/**
  * 函    数：将OLED显存数组全部取反
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Reverse(void)
{
    uint8_t i, j;
    for (j = 0; j < 2; j ++)
    {
        for (i = 0; i < 96; i ++)
        {
            OLED_DisplayBuf[j][i] ^= 0xFF;
        }
    }
}
    
/**
  * 函    数：将OLED显存数组部分取反
  * 参    数：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Width 指定区域的宽度，范围：0~96
  * 参    数：Height 指定区域的高度，范围：0~16
  * 返 回 值：无
  */
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t i, j;
    
    for (j = Y; j < Y + Height; j ++)
    {
        for (i = X; i < X + Width; i ++)
        {
            if (i >= 0 && i <= 95 && j >= 0 && j <= 15)
            {
                OLED_DisplayBuf[j / 8][i] ^= 0x01 << (j % 8);
            }
        }
    }
}

/**
  * 函    数：OLED显示一个字符
  * 参    数：X 指定字符左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定字符左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Char 指定要显示的字符，范围：ASCII码可见字符
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素 (适合整屏)
  *                 OLED_6X8		宽6像素，高8像素
  * 返 回 值：无
  * 注    意：OLED_8X16字体高度为16，正好占满屏幕高度
  *           OLED_6X8字体高度为8，可以显示两行
  */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
    if (FontSize == OLED_8X16)
    {
        /* 检查是否超出屏幕范围 */
        if (X + 8 > 96 || Y + 16 > 16) return;
        OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[Char - ' ']);
    }
    else if(FontSize == OLED_6X8)
    {
        if (X + 6 > 96 || Y + 8 > 16) return;
        OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[Char - ' ']);
    }
}

/**
  * 函    数：OLED显示字符串
  * 参    数：X 指定字符串左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定字符串左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：String 指定要显示的字符串
  * 参    数：FontSize 指定字体大小
  *           范围：OLED_8X16		宽8像素，高16像素
  *                 OLED_6X8		宽6像素，高8像素
  * 返 回 值：无
  */
void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize)
{
    uint16_t i = 0;
    uint16_t XOffset = 0;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    while (String[i] != '\0')
    {
        if (X + XOffset + charWidth <= 96)
        {
            OLED_ShowChar(X + XOffset, Y, String[i], FontSize);
            XOffset += charWidth;
        }
        else
        {
            break;  // 超出屏幕宽度，停止显示
        }
        i++;
    }
}

/**
  * 函    数：OLED显示数字（十进制，正整数）
  * 参    数：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Number 指定要显示的数字，范围：0~4294967295
  * 参    数：Length 指定数字的长度，范围：0~10
  * 参    数：FontSize 指定字体大小
  */
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    for (i = 0; i < Length; i++)
    {
        if (X + i * charWidth + charWidth <= 96)
        {
            OLED_ShowChar(X + i * charWidth, Y, Number / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
        }
    }
}

/**
  * 函    数：OLED显示有符号数字
  */
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint32_t Number1;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    if (Number >= 0)
    {
        if (X + charWidth <= 96)
        {
            OLED_ShowChar(X, Y, '+', FontSize);
            Number1 = Number;
        }
    }
    else
    {
        if (X + charWidth <= 96)
        {
            OLED_ShowChar(X, Y, '-', FontSize);
            Number1 = -Number;
        }
    }
    
    for (i = 0; i < Length; i++)
    {
        if (X + (i + 1) * charWidth + charWidth <= 96)
        {
            OLED_ShowChar(X + (i + 1) * charWidth, Y, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize);
        }
    }
}

/**
  * 函    数：OLED显示十六进制数字
  */
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i, SingleNumber;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    for (i = 0; i < Length; i++)
    {
        if (X + i * charWidth + charWidth <= 96)
        {
            SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
            
            if (SingleNumber < 10)
            {
                OLED_ShowChar(X + i * charWidth, Y, SingleNumber + '0', FontSize);
            }
            else
            {
                OLED_ShowChar(X + i * charWidth, Y, SingleNumber - 10 + 'A', FontSize);
            }
        }
    }
}

/**
  * 函    数：OLED显示二进制数字
  */
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    uint8_t i;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    for (i = 0; i < Length; i++)
    {
        if (X + i * charWidth + charWidth <= 96)
        {
            OLED_ShowChar(X + i * charWidth, Y, Number / OLED_Pow(2, Length - i - 1) % 2 + '0', FontSize);
        }
    }
}

/**
  * 函    数：OLED显示浮点数字（简化版，不处理小数部分）
  */
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
    /* 简化处理：只显示整数部分和小数点 */
    uint32_t IntNum;
    uint8_t charWidth = (FontSize == OLED_8X16) ? 8 : 6;
    
    if (Number >= 0)
    {
        if (X + charWidth <= 96)
        {
            OLED_ShowChar(X, Y, '+', FontSize);
        }
        IntNum = (uint32_t)Number;
    }
    else
    {
        if (X + charWidth <= 96)
        {
            OLED_ShowChar(X, Y, '-', FontSize);
        }
        IntNum = (uint32_t)(-Number);
    }
    
    /* 显示整数部分 */
    OLED_ShowNum(X + charWidth, Y, IntNum, IntLength, FontSize);
    
    /* 显示小数点 */
    if (X + (IntLength + 1) * charWidth + charWidth <= 96)
    {
        OLED_ShowChar(X + (IntLength + 1) * charWidth, Y, '.', FontSize);
    }
}

/**
  * 函    数：OLED显示图像
  * 参    数：X 指定图像左上角的横坐标，范围：-32768~32767，屏幕区域：0~95
  * 参    数：Y 指定图像左上角的纵坐标，范围：-32768~32767，屏幕区域：0~15
  * 参    数：Width 指定图像的宽度，范围：0~96
  * 参    数：Height 指定图像的高度，范围：0~16
  * 参    数：Image 指定要显示的图像
  */
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
    uint8_t i = 0, j = 0;
    int16_t Page, Shift;
    
    /* 检查图像尺寸是否超出屏幕范围 */
    if (X + Width > 96) Width = 96 - X;
    if (Y + Height > 16) Height = 16 - Y;
    
    /* 将图像所在区域清空 */
    OLED_ClearArea(X, Y, Width, Height);
    
    /* 遍历指定图像涉及的相关页 */
    for (j = 0; j < (Height - 1) / 8 + 1; j ++)
    {
        /* 遍历指定图像涉及的相关列 */
        for (i = 0; i < Width; i ++)
        {
            if (X + i >= 0 && X + i <= 95)
            {
                Page = Y / 8;
                Shift = Y % 8;
                if (Y < 0)
                {
                    Page -= 1;
                    Shift += 8;
                }
                
                if (Page + j >= 0 && Page + j <= 1)
                {
                    OLED_DisplayBuf[Page + j][X + i] |= Image[j * Width + i] << (Shift);
                }
                
                if (Page + j + 1 >= 0 && Page + j + 1 <= 1)
                {                   
                    OLED_DisplayBuf[Page + j + 1][X + i] |= Image[j * Width + i] >> (8 - Shift);
                }
            }
        }
    }
}

/**
  * 函    数：OLED使用printf函数打印格式化字符串
  */
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{
    char String[128];                       // 减小缓冲区大小
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    OLED_ShowString(X, Y, String, FontSize);
}

/**
  * 函    数：OLED使用printf函数打印格式化字符串（支持自动换行）
  */
void OLED_Printf_New(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{
    char String[128];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    
    int16_t currentX = X;
    int16_t currentY = Y;
    uint8_t charWidth = 0;
    uint8_t charHeight = 0;
    uint8_t maxLineChars = 0;
    
    if (FontSize == OLED_8X16)
    {
        charWidth = 8;
        charHeight = 16;
        maxLineChars = 12;                    // 96 / 8 = 12个字符
    }
    else if (FontSize == OLED_6X8)
    {
        charWidth = 6;
        charHeight = 8;
        maxLineChars = 16;                    // 96 / 6 = 16个字符
    }
    
    uint8_t maxLines = 0;
    if (FontSize == OLED_8X16)
    {
        maxLines = 1;                         // 16 / 16 = 1行
    }
    else if (FontSize == OLED_6X8)
    {
        maxLines = 2;                         // 16 / 8 = 2行
    }
    
    for (int i = 0; String[i] != '\0'; i++)
    {
        if (String[i] == '\n')
        {
            currentX = X;
            currentY += charHeight;
            if (currentY + charHeight > 16) break;
            continue;
        }
        
        if (currentX + charWidth > 96 ||
            ((currentX - X) / charWidth) >= maxLineChars)
        {
            currentX = X;
            currentY += charHeight;
            if (currentY + charHeight > 16) break;
        }
        
        if (String[i] >= 0x20 && String[i] <= 0x7E)
        {
            OLED_ShowChar(currentX, currentY, String[i], FontSize);
            currentX += charWidth;
        }
    }
}

/**
  * 函    数：OLED在指定位置画一个点
  */
void OLED_DrawPoint(int16_t X, int16_t Y)
{
    if (X >= 0 && X <= 95 && Y >= 0 && Y <= 15)
    {
        OLED_DisplayBuf[Y / 8][X] |= 0x01 << (Y % 8);
    }
}

/**
  * 函    数：OLED获取指定位置点的值
  */
uint8_t OLED_GetPoint(int16_t X, int16_t Y)
{
    if (X >= 0 && X <= 95 && Y >= 0 && Y <= 15)
    {
        if (OLED_DisplayBuf[Y / 8][X] & 0x01 << (Y % 8))
        {
            return 1;
        }
    }
    return 0;
}

/**
  * 函    数：OLED画线（简化版）
  */
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;
    
    if (y0 == y1)
    {
        if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
        for (x = x0; x <= x1; x ++)
        {
            OLED_DrawPoint(x, y0);
        }
    }
    else if (x0 == x1)
    {
        if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
        for (y = y0; y <= y1; y ++)
        {
            OLED_DrawPoint(x0, y);
        }
    }
    else
    {
        /* Bresenham算法保持不变，但需要注意Y坐标范围限制在0-15 */
        if (x0 > x1)
        {
            temp = x0; x0 = x1; x1 = temp;
            temp = y0; y0 = y1; y1 = temp;
        }
        
        if (y0 > y1)
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }
        
        if (y1 - y0 > x1 - x0)
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
        
        if (yflag && xyflag){OLED_DrawPoint(y, -x);}
        else if (yflag)     {OLED_DrawPoint(x, -y);}
        else if (xyflag)    {OLED_DrawPoint(y, x);}
        else                {OLED_DrawPoint(x, y);}
        
        while (x < x1)
        {
            x ++;
            if (d < 0)
            {
                d += incrE;
            }
            else
            {
                y ++;
                d += incrNE;
            }
            
            if (yflag && xyflag){OLED_DrawPoint(y, -x);}
            else if (yflag)     {OLED_DrawPoint(x, -y);}
            else if (xyflag)    {OLED_DrawPoint(y, x);}
            else                {OLED_DrawPoint(x, y);}
        }   
    }
}

/**
  * 函    数：OLED画虚线(固定样式)
  */
void OLED_DrawDashedLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
    const uint8_t dashLength = 3;
    const uint8_t gapLength = 2;
    
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;
    uint16_t pointCount = 0;
    
    if (y0 == y1)
    {
        if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
        
        for (x = x0; x <= x1; x++)
        {
            if (pointCount < dashLength)
            {
                OLED_DrawPoint(x, y0);
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
    else if (x0 == x1)
    {
        if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
        
        for (y = y0; y <= y1; y++)
        {
            if (pointCount < dashLength)
            {
                OLED_DrawPoint(x0, y);
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
    else
    {
        /* 斜线部分代码保持不变，添加虚线判断 */
        if (x0 > x1)
        {
            temp = x0; x0 = x1; x1 = temp;
            temp = y0; y0 = y1; y1 = temp;
        }
        
        if (y0 > y1)
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }
        
        if (y1 - y0 > x1 - x0)
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
        
        if (pointCount < dashLength)
        {
            if (yflag && xyflag){OLED_DrawPoint(y, -x);}
            else if (yflag)     {OLED_DrawPoint(x, -y);}
            else if (xyflag)    {OLED_DrawPoint(y, x);}
            else                {OLED_DrawPoint(x, y);}
        }
        pointCount = (pointCount + 1) % (dashLength + gapLength);
        
        while (x < x1)
        {
            x++;
            if (d < 0)
            {
                d += incrE;
            }
            else
            {
                y++;
                d += incrNE;
            }
            
            if (pointCount < dashLength)
            {
                if (yflag && xyflag){OLED_DrawPoint(y, -x);}
                else if (yflag)     {OLED_DrawPoint(x, -y);}
                else if (xyflag)    {OLED_DrawPoint(y, x);}
                else                {OLED_DrawPoint(x, y);}
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
}

/**
  * 函    数：OLED矩形
  */
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
    int16_t i, j;
    
    /* 检查边界 */
    if (X + Width > 96) Width = 96 - X;
    if (Y + Height > 16) Height = 16 - Y;
    
    if (!IsFilled)
    {
        for (i = X; i < X + Width; i ++)
        {
            OLED_DrawPoint(i, Y);
            OLED_DrawPoint(i, Y + Height - 1);
        }
        for (i = Y; i < Y + Height; i ++)
        {
            OLED_DrawPoint(X, i);
            OLED_DrawPoint(X + Width - 1, i);
        }
    }
    else
    {
        for (i = X; i < X + Width; i ++)
        {
            for (j = Y; j < Y + Height; j ++)
            {
                OLED_DrawPoint(i, j);
            }
        }
    }
}

/**
  * 函    数：OLED三角形（简化版，只支持不填充）
  */
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
    if (!IsFilled)
    {
        OLED_DrawLine(X0, Y0, X1, Y1);
        OLED_DrawLine(X0, Y0, X2, Y2);
        OLED_DrawLine(X1, Y1, X2, Y2);
    }
    else
    {
        /* 简化处理：只画边框 */
        OLED_DrawLine(X0, Y0, X1, Y1);
        OLED_DrawLine(X0, Y0, X2, Y2);
        OLED_DrawLine(X1, Y1, X2, Y2);
    }
}

/**
  * 函    数：OLED画圆（简化版，半径限制）
  */
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled)
{
    int16_t x, y, d, j;
    
    /* 限制半径，避免超出屏幕 */
    if (Radius > 8) Radius = 8;
    
    d = 1 - Radius;
    x = 0;
    y = Radius;
    
    OLED_DrawPoint(X + x, Y + y);
    OLED_DrawPoint(X - x, Y - y);
    OLED_DrawPoint(X + y, Y + x);
    OLED_DrawPoint(X - y, Y - x);
    
    while (x < y)
    {
        x ++;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y --;
            d += 2 * (x - y) + 1;
        }
        
        OLED_DrawPoint(X + x, Y + y);
        OLED_DrawPoint(X + y, Y + x);
        OLED_DrawPoint(X - x, Y - y);
        OLED_DrawPoint(X - y, Y - x);
        OLED_DrawPoint(X + x, Y - y);
        OLED_DrawPoint(X + y, Y - x);
        OLED_DrawPoint(X - x, Y + y);
        OLED_DrawPoint(X - y, Y + x);
    }
}

/*********************功能函数*/

/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com****************/