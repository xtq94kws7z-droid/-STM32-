/**
 * main.c - 回退到确切能用的版本
 * 公式完全复刻 Bosch 64-bit 算法，分开三个函数
 */

#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"
#include "soft_i2c.h"
#include "bme280.h"
#include "chinese_font.h"
#include "oled.h"
#include <stdio.h>

static char line[22];

/* 本地校准数据 */
static struct {
    uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
    uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3; int16_t dig_P4;
    int16_t dig_P5; int16_t dig_P6; int16_t dig_P7; int16_t dig_P8; int16_t dig_P9;
    uint8_t dig_H1; int16_t dig_H2; uint8_t dig_H3; int16_t dig_H4; int16_t dig_H5; int8_t dig_H6;
} cal;

void SystemClock_Config(void);

/* ========== Bosch 官方公式（三个独立函数）========== */

static int32_t t_fine;

static int32_t comp_temp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)cal.dig_T1 << 1))) * ((int32_t)cal.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)cal.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)cal.dig_T1))) >> 12) *
            ((int32_t)cal.dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

static uint32_t comp_press(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)cal.dig_P3) >> 8) +
           ((var1 * (int64_t)cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)cal.dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - (int64_t)adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal.dig_P7) << 4);
    return (uint32_t)(p / 256);     /* p/25600 * 100 */
}

static uint32_t comp_hum(int32_t adc_H)
{
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - (int32_t)76800);
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)cal.dig_H4) << 20) -
                    (((int32_t)cal.dig_H5) * v_x1_u32r)) +
                   ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)cal.dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)cal.dig_H3)) >> 11) +
                       (int32_t)32768)) >> 10) +
                    ((int32_t)2097152)) *
                   ((int32_t)cal.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                                ((int32_t)cal.dig_H1)) >> 4));
    if (v_x1_u32r < 0) v_x1_u32r = 0;
    if (v_x1_u32r > 419430400) v_x1_u32r = 419430400;
    return v_x1_u32r >> 12;   /* %RH * 1024 */
}

/* ========== main ========== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    SoftI2C_Init();
    MX_USART1_UART_Init();

    uint32_t temp, hum, press;
    uint32_t rt, rp, rh;
    uint8_t raw[8], buf[26], hbuf[7], id;

    OLED_Init(&hi2c1);

    /* 读校准 */
    SoftI2C_ReadRegs(BME280_ADDR, 0x88, buf, 26);
    SoftI2C_ReadRegs(BME280_ADDR, 0xE1, hbuf, 7);

    cal.dig_T1 = (uint16_t)(buf[0] | (buf[1] << 8));
    cal.dig_T2 = (int16_t)(buf[2] | (buf[3] << 8));
    cal.dig_T3 = (int16_t)(buf[4] | (buf[5] << 8));
    cal.dig_P1 = (uint16_t)(buf[6] | (buf[7] << 8));
    cal.dig_P2 = (int16_t)(buf[8] | (buf[9] << 8));
    cal.dig_P3 = (int16_t)(buf[10] | (buf[11] << 8));
    cal.dig_P4 = (int16_t)(buf[12] | (buf[13] << 8));
    cal.dig_P5 = (int16_t)(buf[14] | (buf[15] << 8));
    cal.dig_P6 = (int16_t)(buf[16] | (buf[17] << 8));
    cal.dig_P7 = (int16_t)(buf[18] | (buf[19] << 8));
    cal.dig_P8 = (int16_t)(buf[20] | (buf[21] << 8));
    cal.dig_P9 = (int16_t)(buf[22] | (buf[23] << 8));
    cal.dig_H1 = buf[25];
    cal.dig_H2 = (int16_t)(hbuf[0] | (hbuf[1] << 8));
    cal.dig_H3 = hbuf[2];
    cal.dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    cal.dig_H5 = (int16_t)((hbuf[4] >> 4) | (hbuf[5] << 4));
    cal.dig_H6 = (int8_t)hbuf[6];

    /* 初始化 Normal 模式 */
    SoftI2C_WriteReg(BME280_ADDR, 0xF2, 0x01);
    SoftI2C_WriteReg(BME280_ADDR, 0xF5, 0x00);
    SoftI2C_WriteReg(BME280_ADDR, 0xF4, 0x27);
    HAL_Delay(20);

    /* 开机显示校准值 */
    sprintf(line, "T1=%u", cal.dig_T1);
    OLED_Print(0, 0, line);
    sprintf(line, "P1=%u", cal.dig_P1);
    OLED_Print(0, 12, line);
    sprintf(line, "H2=%d", cal.dig_H2);
    OLED_Print(0, 24, line);
    OLED_Update();
    HAL_Delay(2000);

    while (1)
    {
        /* 读8字节（一次I2C） */
        if (SoftI2C_ReadRegs(BME280_ADDR, 0xF7, raw, 8) != 0) {
            OLED_Clear();
            OLED_Print(0, 0, "I2C FAIL");
            OLED_Update();
            HAL_Delay(2000);
            continue;
        }

        rp  = ((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((uint32_t)raw[2] >> 4);
        rt  = ((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((uint32_t)raw[5] >> 4);
        rh  = ((uint32_t)raw[6] << 8) | raw[7];

        /* 温度补偿（设 t_fine） */
        /* BME280 原始 ADC 是 20-bit 无符号值，直接转 int32_t 即可 */
        int32_t adc_T = (int32_t)rt;
        int32_t adc_P = (int32_t)rp;

        temp = comp_temp(adc_T);  /* 温度 (°C * 100) 并设置 t_fine */
        press = comp_press(adc_P); /* 气压 (hPa * 100) */
        hum = comp_hum((int32_t)rh);  /* 湿度 (%RH * 1024) */
        /* 显示：汉字 + ASCII */
        OLED_Clear();
        OLED_DrawChinese(0, 0, CN_CHAR_WEN);  /* 温 */
        OLED_DrawChinese(0, 16, CN_CHAR_SHI); /* 湿 */
        OLED_DrawChinese(0, 32, CN_CHAR_YA);  /* 压 */
        sprintf(line, "%d.%dC", temp/100, temp%100);
        OLED_Print(18, 4, line);
        sprintf(line, "%d.%d%%", hum/1024, (hum%1024)*100/1024);
        OLED_Print(18, 20, line);
        sprintf(line, "%d.%dhPa", press/100, press%100);
        OLED_Print(18, 36, line);
        OLED_Update();

        HAL_Delay(2000);
    }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
    return ch;
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
