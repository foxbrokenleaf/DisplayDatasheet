/***************************************************************************************
  * 程序名称：				IST3931 LCD显示屏驱动程序
  * 屏幕分辨率：				144x65
  * 驱动芯片：				IST3931
  * 通信协议：				I2C
  * 引脚数量：               14Pin
  * 程序创建时间：			2025.03.16
  * 说明：修复显示内容自动消失的问题，添加基于DrawPoint的显示函数
  ***************************************************************************************
  */

#include "stm32f10x.h"
#include "ist3931.h"
#include "ist3931_font.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

/* 全局变量 - 显存数组 */
uint8_t IST3931_DisplayBuf[65][18];  // [页][列]，9页 * 144列

/* 全局标志 - 记录显示状态 */
static uint8_t display_on = 1;

/* GPIO引脚定义 */
#define IST3931_SCL_PORT        GPIOB
#define IST3931_SCL_PIN         GPIO_Pin_6
#define IST3931_SDA_PORT        GPIOB
#define IST3931_SDA_PIN         GPIO_Pin_7
#define IST3931_RST_PORT        GPIOB
#define IST3931_RST_PIN         GPIO_Pin_8

/* I2C延时参数 */
#define I2C_DELAY_US            2

/* 应答标志 - 使用volatile避免优化警告 */
static volatile uint8_t ack = 1;

// 行映射表：逻辑行0~31对应的物理行索引
// 根据您提供的规律：第1行(逻辑行0) -> 物理行0
// 第2行(逻辑行1) -> 物理行17
// 第3行(逻辑行2) -> 物理行1
// 第4行(逻辑行3) -> 物理行18
// ...
const uint8_t RowMap[32] = {
    0,  17, 1,  18, 2,  19, 3,  20,  // 逻辑行0-7
    4,  21, 5,  22, 6,  23, 7,  24,  // 逻辑行8-15
    8,  25, 9,  26, 10, 27, 11, 28,  // 逻辑行16-23
    12, 29, 13, 30, 14, 31, 15, 32   // 逻辑行24-31
};

// 获取物理行号的辅助函数
#define GET_PHYSICAL_ROW(logic_row) ((logic_row) < 32 ? RowMap[logic_row] : (logic_row))

/* 延时函数 */
void IST3931_DelayUs(uint32_t us)
{
    uint32_t i;
    for (i = 0; i < us * 8; i++)
    {
        __NOP();
    }
}

void IST3931_DelayMs(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++)
    {
        IST3931_DelayUs(1000);
    }
}

/* GPIO控制函数 */
void IST3931_W_SCL(uint8_t BitValue)
{
    GPIO_WriteBit(IST3931_SCL_PORT, IST3931_SCL_PIN, (BitAction)BitValue);
}

void IST3931_W_SDA(uint8_t BitValue)
{
    GPIO_WriteBit(IST3931_SDA_PORT, IST3931_SDA_PIN, (BitAction)BitValue);
}

void IST3931_W_RST(uint8_t BitValue)
{
    GPIO_WriteBit(IST3931_RST_PORT, IST3931_RST_PIN, (BitAction)BitValue);
}

uint8_t IST3931_R_SDA(void)
{
    return GPIO_ReadInputDataBit(IST3931_SDA_PORT, IST3931_SDA_PIN);
}

/* GPIO初始化 */
void IST3931_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = IST3931_SCL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IST3931_SCL_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = IST3931_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IST3931_SDA_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = IST3931_RST_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IST3931_RST_PORT, &GPIO_InitStructure);
    
    IST3931_W_SCL(1);
    IST3931_W_SDA(1);
    IST3931_W_RST(1);
    
    IST3931_DelayMs(50);
}

/* I2C协议函数 */
void iic_start(void)
{
    IST3931_W_SDA(1);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SCL(1);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SDA(0);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SCL(0);
    IST3931_DelayUs(I2C_DELAY_US);
}

void iic_stop(void)
{
    IST3931_W_SDA(0);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SCL(1);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SDA(1);
    IST3931_DelayUs(I2C_DELAY_US);
}

void IICSendByte(uint8_t Byte)
{
    uint8_t i;
    
    for (i = 0; i < 8; i++)
    {
        if (Byte & 0x80)
            IST3931_W_SDA(1);
        else
            IST3931_W_SDA(0);
        
        Byte <<= 1;
        
        IST3931_DelayUs(I2C_DELAY_US);
        IST3931_W_SCL(1);
        IST3931_DelayUs(I2C_DELAY_US);
        IST3931_W_SCL(0);
        IST3931_DelayUs(I2C_DELAY_US);
    }
}

void check_ACK(void)
{
    IST3931_W_SDA(1);
    IST3931_DelayUs(I2C_DELAY_US);
    IST3931_W_SCL(1);
    IST3931_DelayUs(I2C_DELAY_US);
    
    if (IST3931_R_SDA() == 1)
        ack = 0;
    else
        ack = 1;
    
    IST3931_W_SCL(0);
    IST3931_DelayUs(I2C_DELAY_US);
}

/* 硬件复位 */
void IST3931_HardwareReset(void)
{
    IST3931_W_RST(0);
    IST3931_DelayMs(10);
    IST3931_W_RST(1);
    IST3931_DelayMs(10);
}

/* 写命令 - 修正：使用IST3931_ADDR代替IST3931_SLAVE_ADDR */
void IST3931_WriteCommand(uint8_t Command)
{
    iic_start();
    IICSendByte(IST3931_ADDR);
    check_ACK();
    IICSendByte(0x80);
    check_ACK();
    IICSendByte(Command);
    check_ACK();
    iic_stop();
}

/* 写数据 - 修正：使用IST3931_ADDR代替IST3931_SLAVE_ADDR */
void IST3931_WriteData(uint8_t Data)
{
    iic_start();
    IICSendByte(IST3931_ADDR);
    check_ACK();
    IICSendByte(0xc0);
    check_ACK();
    IICSendByte(Data);
    check_ACK();
    iic_stop();
}

/* 写多个数据 - 修正：使用IST3931_ADDR代替IST3931_SLAVE_ADDR */
void IST3931_WriteDataMulti(uint8_t *Data, uint16_t Count)
{
    uint16_t i;
    
    iic_start();
    IICSendByte(IST3931_ADDR);
    check_ACK();
    IICSendByte(0xc0);
    check_ACK();
    
    for (i = 0; i < Count; i++)
    {
        IICSendByte(Data[i]);
        check_ACK();
    }
    
    iic_stop();
}


/* 设置光标位置 - Y是物理行地址 */
void IST3931_SetCursor(uint8_t Y, uint8_t X)
{
    /* Y: 物理行地址 (0-64)
       X: 列地址，8个列为一个地址 (0-17) */
    
    IST3931_WriteCommand(0x10 + (Y >> 4));      // 设置行地址高3位
    IST3931_WriteCommand(0x00 + (Y & 0x0F));    // 设置行地址低4位
    IST3931_WriteCommand(0xc0 + X);              // 设置列地址
}

/* 初始化序列 */
void Write_ist3931_init1(void)
{
    IST3931_WriteCommand(0x3a);  // 开启时钟振荡
    IST3931_WriteCommand(0x61);  // 行排列方式【正常】
    IST3931_WriteCommand(0x2f);  // 升压器开
    IST3931_WriteCommand(0xb1);  // 设置升压电压
    IST3931_WriteCommand(200);    // 设置升压电压值(对比度)
    IST3931_WriteCommand(0x34);  // LCD Blass（亮度）
    IST3931_WriteCommand(0x62);  // 行、列方向，全亮
    IST3931_WriteCommand(0x91);  // 占空比低4位
    IST3931_WriteCommand(0xa2);  // 占空比高3位
    IST3931_WriteCommand(0x40);  // 起始行低4位
    IST3931_WriteCommand(0x50);  // 起始行高3位
    IST3931_WriteCommand(0x3d);  // 开显示
}

/* 设置正常显示模式 */
void IST3931_SetNormalMode(void)
{
    IST3931_WriteCommand(0x60);  // 正常显示
}

/* 清屏函数 - 使用物理地址 */
void ist3931_disp_clear(void)
{
    uint16_t i;
    uint8_t y;
    
    /* 需要遍历所有65行 */
    for (y = 0; y < 65; y++)
    {
        IST3931_SetCursor(y, 0);
        
        /* 每行有18个列地址（144/8=18）*/
        for (i = 0; i < 18; i++)
        {
            IST3931_WriteData(0x00);
        }
    }
}

/* 主初始化函数 */
void IST3931_Init(void)
{
    IST3931_GPIO_Init();
    IST3931_HardwareReset();
    IST3931_DelayMs(50);
    
    Write_ist3931_init1();
    IST3931_DelayMs(20);
    
    IST3931_SetNormalMode();
    IST3931_DelayMs(10);
    
    ist3931_disp_clear();
    IST3931_Clear();
    
    display_on = 1;
}

/* 清空显存 - 显存中存储的是逻辑坐标的数据 */
void IST3931_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 65; j++)
    {
        for (i = 0; i < 18; i++)
        {
            IST3931_DisplayBuf[j][i] = 0x00;
        }
    }
}

/**
  * 函    数：更新全屏 - 考虑隔行扫描映射
  * 参    数：无
  * 返 回 值：无
  */
void IST3931_Update(void)
{
    uint8_t i, j;
    
    // 设置起始地址（行0，列0）
    IST3931_WriteCommand(0x10);  // 行地址高3位
    IST3931_WriteCommand(0x00);  // 行地址低4位
    IST3931_WriteCommand(0xc0);  // 列地址起始
    
    // 一次性写入所有显存数据
    for(i = 0; i < 65; i++)
    {
        for(j = 0; j < 18; j++)
        {
            IST3931_WriteData(IST3931_DisplayBuf[i][j]);
        }
    }
}

/**
  * 函    数：更新指定区域 - 修正参数类型匹配
  * 参    数：X 指定区域左上角的横坐标
  * 参    数：Y 指定区域左上角的纵坐标
  * 参    数：Width 指定区域的宽度
  * 参    数：Height 指定区域的高度
  * 返 回 值：无
  */
void IST3931_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{

}

/**
  * 函    数：清空指定区域
  * 参    数：X 指定区域左上角的横坐标
  * 参    数：Y 指定区域左上角的纵坐标
  * 参    数：Width 指定区域的宽度
  * 参    数：Height 指定区域的高度
  * 返 回 值：无
  */
void IST3931_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t i, j;
    
    for (j = Y; j < Y + Height; j++)
    {
        for (i = X; i < X + Width; i++)
        {
            if (i >= 0 && i < IST3931_WIDTH && j >= 0 && j < IST3931_HEIGHT)
            {
                IST3931_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
            }
        }
    }
}

/*============================================================================
 * 基于DrawPoint的显示函数
 *============================================================================*/

/**
  * 函    数：IST3931在指定位置画一个点
  * 参    数：X 横坐标，范围：0~143
  * 参    数：Y 纵坐标（逻辑行），范围：0~31
  * 返 回 值：无
  */
void IST3931_DrawPoint(int16_t X, int16_t Y)
{
    uint8_t physical_row, col_byte, bit_mask;
    
    if (X < 0 || X >= 144 || Y < 0 || Y >= 32) return;
    
    physical_row = GET_PHYSICAL_ROW(Y);
    col_byte = X / 8;
    if (col_byte >= 18) return;
    
    bit_mask = 0x80 >> (X % 8);
    IST3931_DisplayBuf[physical_row][col_byte] |= bit_mask;
}

/**
  * 函    数：IST3931获取指定位置点的值
  * 参    数：X 指定点的横坐标，范围：0~143
  * 参    数：Y 指定点的纵坐标，范围：0~64
  * 返 回 值：该点的值，0或1
  */
uint8_t IST3931_GetPoint(int16_t X, int16_t Y)
{
    if (X >= 0 && X < IST3931_WIDTH && Y >= 0 && Y < IST3931_HEIGHT)
    {
        if (IST3931_DisplayBuf[Y / 8][X] & (0x01 << (Y % 8)))
        {
            return 1;
        }
    }
    return 0;
}

/**
  * 函    数：将IST3931显存数组全部取反
  * 参    数：无
  * 返 回 值：无
  */
void IST3931_Reverse(void)
{
    uint8_t i, j;
    for (j = 0; j < 9; j++)
    {
        for (i = 0; i < 144; i++)
        {
            IST3931_DisplayBuf[j][i] ^= 0xFF;
        }
    }
}

/**
  * 函    数：将IST3931显存数组部分取反
  * 参    数：X 指定区域左上角的横坐标，范围：0~143
  * 参    数：Y 指定区域左上角的纵坐标，范围：0~64
  * 参    数：Width 指定区域的宽度，范围：0~144
  * 参    数：Height 指定区域的高度，范围：0~65
  * 返 回 值：无
  */
void IST3931_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
    int16_t i, j;
    
    for (j = Y; j < Y + Height; j++)
    {
        for (i = X; i < X + Width; i++)
        {
            if (i >= 0 && i < IST3931_WIDTH && j >= 0 && j < IST3931_HEIGHT)
            {
                IST3931_DisplayBuf[j / 8][i] ^= 0x01 << (j % 8);
            }
        }
    }
}

/**
  * 函    数：次方函数
  * 参    数：X 底数
  * 参    数：Y 指数
  * 返 回 值：等于X的Y次方
  */
uint32_t IST3931_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

/**
  * 函    数：IST3931在指定位置显示一个字符
  * 参    数：X 横坐标，范围：0~143
  * 参    数：Y 纵坐标（逻辑行），范围：0~31
  * 参    数：Char 要显示的ASCII字符
  * 参    数：FontSize 字体大小，6（6x8）或 8（8x16）
  */
void IST3931_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
    uint8_t i = Y;
    if(FontSize == IST3931_8X16){
        for(i = Y;i < Y + 16;i++){
            IST3931_DisplayBuf[RowMap[i]][(uint8_t)(X / 8)] = IST3931_F8x16[Char - 32][(Y + 15) - i];
        }
    }
    if(FontSize == IST3931_6X8){
        for(i = Y;i < Y + 6;i++){
            IST3931_DisplayBuf[RowMap[i]][(uint8_t)(X / 6)] = IST3931_F6x8[Char - 32][(Y + 5) - i];
        }        
    }

}

/**
  * 函    数：IST3931显示字符串
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：String 要显示的字符串
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize)
{
    uint8_t width = (FontSize == 6) ? 6 : 8;
    
    while (*String != '\0')
    {
        if (X + width > 144) break;
        IST3931_ShowChar(X, Y, *String++, FontSize);
        X += width;
    }
}

/**
  * 函    数：IST3931显示无符号整数
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Number 要显示的数值
  * 参    数：Length 显示位数（不足补空格，超出显示全部）
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    char numStr[12];
    uint8_t i, len;
    uint8_t width = (FontSize == 6) ? 6 : 8;
    
    sprintf(numStr, "%lu", Number);
    len = strlen(numStr);
    
    if (Length > len)
    {
        for (i = 0; i < Length - len; i++)
        {
            IST3931_ShowChar(X + i * width, Y, '0', FontSize);
        }
        X += (Length - len) * width;
    }
    
    IST3931_ShowString(X, Y, numStr, FontSize);
}

/**
  * 函    数：IST3931显示有符号整数
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Number 要显示的数值
  * 参    数：Length 显示位数（包括符号位）
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
    char numStr[12];
    uint8_t i, len;
    uint8_t width = (FontSize == 6) ? 6 : 8;
    
    sprintf(numStr, "%ld", Number);
    len = strlen(numStr);
    
    if (Length > len)
    {
        for (i = 0; i < Length - len; i++)
        {
            IST3931_ShowChar(X + i * width, Y, ' ', FontSize);
        }
        X += (Length - len) * width;
    }
    
    IST3931_ShowString(X, Y, numStr, FontSize);
}

/**
  * 函    数：IST3931显示十六进制数
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Number 要显示的数值
  * 参    数：Length 显示位数（不足补0）
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    char numStr[9];
    uint8_t i, len;
    uint8_t width = (FontSize == 6) ? 6 : 8;
    
    sprintf(numStr, "%lX", Number);
    len = strlen(numStr);
    
    if (Length > len)
    {
        for (i = 0; i < Length - len; i++)
        {
            IST3931_ShowChar(X + i * width, Y, '0', FontSize);
        }
        X += (Length - len) * width;
    }
    
    IST3931_ShowString(X, Y, numStr, FontSize);
}

/**
  * 函    数：IST3931显示二进制数
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Number 要显示的数值
  * 参    数：Length 显示位数
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
    char numStr[33];
    uint8_t i;
    
    for (i = 0; i < Length; i++)
    {
        numStr[Length - 1 - i] = (Number & (1UL << i)) ? '1' : '0';
    }
    numStr[Length] = '\0';
    
    IST3931_ShowString(X, Y, numStr, FontSize);
}

/**
  * 函    数：IST3931显示浮点数
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Number 要显示的数值
  * 参    数：IntLength 整数部分位数
  * 参    数：FraLength 小数部分位数
  * 参    数：FontSize 字体大小
  */
void IST3931_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
    char numStr[20];
    
    sprintf(numStr, "%*.*f", IntLength + FraLength + 1, FraLength, Number);
    IST3931_ShowString(X, Y, numStr, FontSize);
}

/**
  * 函    数：IST3931使用printf函数打印格式化字符串（基于DrawPoint）
  * 参    数：X 指定字符串左上角的横坐标
  * 参    数：Y 指定字符串左上角的纵坐标
  * 参    数：FontSize 指定字体大小
  * 参    数：format 格式化字符串
  * 参    数：... 可变参数
  * 返 回 值：无
  */
void IST3931_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{

}

/**
  * 函    数：IST3931使用printf函数打印格式化字符串（支持自动换行）
  * 参    数：X 指定字符串左上角的横坐标
  * 参    数：Y 指定字符串左上角的纵坐标
  * 参    数：FontSize 指定字体大小
  * 参    数：format 格式化字符串
  * 参    数：... 可变参数
  * 返 回 值：无
  */
void IST3931_Printf_New(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...)
{

}

/**
  * 函    数：IST3931画虚线（固定样式）
  * 参    数：X0 起点横坐标
  * 参    数：Y0 起点纵坐标
  * 参    数：X1 终点横坐标
  * 参    数：Y1 终点纵坐标
  * 返 回 值：无
  */
void IST3931_DrawDashedLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
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
                IST3931_DrawPoint(x, y0);
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
                IST3931_DrawPoint(x0, y);
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
    else
    {
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
            if (yflag && xyflag){IST3931_DrawPoint(y, -x);}
            else if (yflag)     {IST3931_DrawPoint(x, -y);}
            else if (xyflag)    {IST3931_DrawPoint(y, x);}
            else                {IST3931_DrawPoint(x, y);}
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
                if (yflag && xyflag){IST3931_DrawPoint(y, -x);}
                else if (yflag)     {IST3931_DrawPoint(x, -y);}
                else if (xyflag)    {IST3931_DrawPoint(y, x);}
                else                {IST3931_DrawPoint(x, y);}
            }
            pointCount = (pointCount + 1) % (dashLength + gapLength);
        }
    }
}

/**
  * 函    数：IST3931画矩形
  * 参    数：X 矩形左上角横坐标
  * 参    数：Y 矩形左上角纵坐标
  * 参    数：Width 矩形宽度
  * 参    数：Height 矩形高度
  * 参    数：IsFilled 是否填充
  * 返 回 值：无
  */
void IST3931_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
    int16_t i, j;
    
    if (X + Width > IST3931_WIDTH) Width = IST3931_WIDTH - X;
    if (Y + Height > IST3931_HEIGHT) Height = IST3931_HEIGHT - Y;
    
    if (!IsFilled)
    {
        for (i = X; i < X + Width; i++)
        {
            IST3931_DrawPoint(i, Y);
            IST3931_DrawPoint(i, Y + Height - 1);
        }
        for (i = Y; i < Y + Height; i++)
        {
            IST3931_DrawPoint(X, i);
            IST3931_DrawPoint(X + Width - 1, i);
        }
    }
    else
    {
        for (i = X; i < X + Width; i++)
        {
            for (j = Y; j < Y + Height; j++)
            {
                IST3931_DrawPoint(i, j);
            }
        }
    }
}

/**
  * 函    数：IST3931画三角形
  * 参    数：X0,Y0 第一个顶点坐标
  * 参    数：X1,Y1 第二个顶点坐标
  * 参    数：X2,Y2 第三个顶点坐标
  * 参    数：IsFilled 是否填充
  * 返 回 值：无
  */
void IST3931_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
    if (!IsFilled)
    {
        IST3931_DrawLine(X0, Y0, X1, Y1);
        IST3931_DrawLine(X0, Y0, X2, Y2);
        IST3931_DrawLine(X1, Y1, X2, Y2);
    }
    else
    {
        /* 简化处理：只画边框 */
        IST3931_DrawLine(X0, Y0, X1, Y1);
        IST3931_DrawLine(X0, Y0, X2, Y2);
        IST3931_DrawLine(X1, Y1, X2, Y2);
    }
}

/**
  * 函    数：IST3931画圆
  * 参    数：X,Y 圆心坐标
  * 参    数：Radius 半径
  * 参    数：IsFilled 是否填充
  * 返 回 值：无
  */
void IST3931_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled)
{
    int16_t x, y, d;
    
    /* 限制半径，避免超出屏幕 */
    if (Radius > 32) Radius = 32;
    
    if (!IsFilled)
    {
        d = 1 - Radius;
        x = 0;
        y = Radius;
        
        while (x <= y)
        {
            IST3931_DrawPoint(X + x, Y + y);
            IST3931_DrawPoint(X - x, Y - y);
            IST3931_DrawPoint(X + y, Y + x);
            IST3931_DrawPoint(X - y, Y - x);
            IST3931_DrawPoint(X + x, Y - y);
            IST3931_DrawPoint(X - x, Y + y);
            IST3931_DrawPoint(X + y, Y - x);
            IST3931_DrawPoint(X - y, Y + x);
            
            if (d < 0)
            {
                d += 2 * x + 1;
            }
            else
            {
                d += 2 * (x - y) + 1;
                y--;
            }
            x++;
        }
    }
    else
    {
        /* 简化的填充圆：画一系列同心圆 */
        for (uint8_t r = 0; r <= Radius; r++)
        {
            d = 1 - r;
            x = 0;
            y = r;
            
            while (x <= y)
            {
                for (int16_t i = X - x; i <= X + x; i++)
                {
                    IST3931_DrawPoint(i, Y + y);
                    IST3931_DrawPoint(i, Y - y);
                }
                for (int16_t i = X - y; i <= X + y; i++)
                {
                    IST3931_DrawPoint(i, Y + x);
                    IST3931_DrawPoint(i, Y - x);
                }
                
                if (d < 0)
                {
                    d += 2 * x + 1;
                }
                else
                {
                    d += 2 * (x - y) + 1;
                    y--;
                }
                x++;
            }
        }
    }
}

/**
  * 函    数：IST3931显示图像
  * 参    数：X 起始横坐标
  * 参    数：Y 起始纵坐标（逻辑行）
  * 参    数：Width 图像宽度
  * 参    数：Height 图像高度
  * 参    数：Image 图像数据
  */
void IST3931_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t Image[][8])
{
    for(uint8_t i = Y;i < (Y + Height) / 2;i++){
        for(uint8_t j = X;j < (X + Width) / 8;j++){
            IST3931_DisplayBuf[RowMap[i]][j] = Image[(((Y + Height) / 2) - 1) - i][j];
        }
    }
}

/* 设置对比度 */
void IST3931_SetContrast(uint8_t Contrast)
{
    IST3931_WriteCommand(0xb1);
    IST3931_WriteCommand(Contrast);
}

/* 设置偏压（亮度） */
void IST3931_SetBias(uint8_t Bias)
{
    if (Bias < 0x30) Bias = 0x30;
    if (Bias > 0x37) Bias = 0x37;
    IST3931_WriteCommand(Bias);
}

/* 设置显示模式 */
void IST3931_SetDisplayMode(uint8_t Mode)
{
    IST3931_WriteCommand(Mode);
}

/* 开启显示 - 增强版，确保显示保持 */
void IST3931_DisplayOn(void)
{
    IST3931_WriteCommand(0x3d);  // 开显示
    display_on = 1;
    
    /* 开显示后，立即刷新一次屏幕，确保内容显示 */
    IST3931_Update();
}

/* 关闭显示 */
void IST3931_DisplayOff(void)
{
    IST3931_WriteCommand(0xae);  // 关显示
    display_on = 0;
}

/**
  * 函    数：设置睡眠模式
  * 参    数：Enable 1-进入睡眠模式，0-退出睡眠模式
  * 返 回 值：无
  */
void IST3931_SetSleepMode(uint8_t Enable)
{
    if (Enable)
    {
        IST3931_WriteCommand(0xae);  // 关闭显示
    }
    else
    {
        IST3931_WriteCommand(0x3d);  // 开启显示
    }
}

/**
  * 函    数：画线函数 - 修正参数类型匹配
  * 参    数：X0 起点横坐标
  * 参    数：Y0 起点纵坐标
  * 参    数：X1 终点横坐标
  * 参    数：Y1 终点纵坐标
  * 返 回 值：无
  */
void IST3931_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
    int16_t dx = X1 > X0 ? X1 - X0 : X0 - X1;
    int16_t dy = Y1 > Y0 ? Y1 - Y0 : Y0 - Y1;
    int16_t steps = dx > dy ? dx : dy;
    
    if (steps == 0)
    {
        IST3931_DrawPoint(X0, Y0);
        return;
    }
    
    for (int16_t i = 0; i <= steps; i++)
    {
        int16_t x = X0 + (int32_t)(X1 - X0) * i / steps;
        int16_t y = Y0 + (int32_t)(Y1 - Y0) * i / steps;
        IST3931_DrawPoint(x, y);
    }
}

/**
  * 函    数：刷新显示 - 保持屏幕内容
  * 说    明：在需要保持显示时调用此函数
  */
void IST3931_Refresh(void)
{
    if (display_on)
    {
        IST3931_Update();
    }
}


void IST3931_Test_2(uint8_t FontIndex, uint8_t DisplayIndex){
    
    IST3931_DisplayBuf[8][DisplayIndex] = IST3931_F6x8[FontIndex][5];
    IST3931_DisplayBuf[25][DisplayIndex] = IST3931_F6x8[FontIndex][4];
    IST3931_DisplayBuf[9][DisplayIndex] = IST3931_F6x8[FontIndex][3];
    IST3931_DisplayBuf[26][DisplayIndex] = IST3931_F6x8[FontIndex][2];
    IST3931_DisplayBuf[10][DisplayIndex] = IST3931_F6x8[FontIndex][1];
    IST3931_DisplayBuf[27][DisplayIndex] = IST3931_F6x8[FontIndex][0];       
}

void IST3931_Test(uint8_t FontIndex, uint8_t DisplayIndex){
    
    IST3931_DisplayBuf[0][DisplayIndex] = IST3931_F8x16[FontIndex][15];
    IST3931_DisplayBuf[17][DisplayIndex] = IST3931_F8x16[FontIndex][14];
    IST3931_DisplayBuf[1][DisplayIndex] = IST3931_F8x16[FontIndex][13];
    IST3931_DisplayBuf[18][DisplayIndex] = IST3931_F8x16[FontIndex][12];
    IST3931_DisplayBuf[2][DisplayIndex] = IST3931_F8x16[FontIndex][11];
    IST3931_DisplayBuf[19][DisplayIndex] = IST3931_F8x16[FontIndex][10];
    IST3931_DisplayBuf[3][DisplayIndex] = IST3931_F8x16[FontIndex][9];
    IST3931_DisplayBuf[20][DisplayIndex] = IST3931_F8x16[FontIndex][8];
    IST3931_DisplayBuf[4][DisplayIndex] = IST3931_F8x16[FontIndex][7];
    IST3931_DisplayBuf[21][DisplayIndex] = IST3931_F8x16[FontIndex][6];
    IST3931_DisplayBuf[5][DisplayIndex] = IST3931_F8x16[FontIndex][5];
    IST3931_DisplayBuf[22][DisplayIndex] = IST3931_F8x16[FontIndex][4];
    IST3931_DisplayBuf[6][DisplayIndex] = IST3931_F8x16[FontIndex][3];
    IST3931_DisplayBuf[23][DisplayIndex] = IST3931_F8x16[FontIndex][2];
    IST3931_DisplayBuf[7][DisplayIndex] = IST3931_F8x16[FontIndex][1];
    IST3931_DisplayBuf[24][DisplayIndex] = IST3931_F8x16[FontIndex][0];       
}