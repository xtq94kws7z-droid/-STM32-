/**
 * bme280.h - BME280 温湿度气压传感器驱动 (I2C)
 * 适用: STM32F103C8T6 + HAL库
 * 通信: I2C1, 地址 0x76 (SDO接GND)
 */

#ifndef __BME280_H
#define __BME280_H

#include "main.h"

/* BME280 I2C 地址 (7位) */
#define BME280_ADDR             0x76    /* SDO=GND */
#define BME280_ADDR_ALT         0x77    /* SDO=VCC */

/* 寄存器地址 */
#define BME280_REG_ID           0xD0    /* 芯片ID, 应读出 0x60 */
#define BME280_REG_RESET        0xE0    /* 软复位 */
#define BME280_REG_CTRL_HUM     0xF2    /* 湿度 oversampling */
#define BME280_REG_STATUS       0xF3    /* 状态寄存器 */
#define BME280_REG_CTRL_MEAS    0xF4    /* 温压 oversampling + 模式 */
#define BME280_REG_CONFIG       0xF5    /* 滤波/等待时间 */
#define BME280_REG_PRESS_MSB    0xF7    /* 气压数据起始 */

/* 校准参数起始地址 */
#define BME280_CALIB_DIG_T1     0x88    /* 温度校准参数区 */
#define BME280_CALIB_DIG_H2     0xE1    /* 湿度校准参数区 */

/* 模式定义 */
#define BME280_MODE_SLEEP       0x00
#define BME280_MODE_FORCED      0x01
#define BME280_MODE_NORMAL      0x03

/* 复位值 */
#define BME280_RESET_CMD        0xB6

/* BME280 校准参数结构体 */
typedef struct {
    /* 温度补偿 (从 0x88-0x9D 读取) */
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    /* 气压补偿 (从 0x8E-0xA1 读取) */
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    /* 湿度补偿 (从 0xE1-0xE7 读取) */
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} BME280_CalibData;

/* 公共 API */
uint8_t  BME280_Init(I2C_HandleTypeDef *hi2c);
uint8_t  BME280_ReadID(I2C_HandleTypeDef *hi2c);
void     BME280_Reset(I2C_HandleTypeDef *hi2c);
uint8_t  BME280_ForceMeasure(I2C_HandleTypeDef *hi2c);

float    BME280_ReadTemperature(I2C_HandleTypeDef *hi2c);
float    BME280_ReadHumidity(I2C_HandleTypeDef *hi2c);
float    BME280_ReadPressure(I2C_HandleTypeDef *hi2c);

/* 一次性读出全部数据，省I2C通信次数 */
uint8_t  BME280_ReadAll(I2C_HandleTypeDef *hi2c,
                        float *temperature, float *humidity, float *pressure);

/* 调试：读取原始ADC值 */
uint8_t  BME280_ReadRaw(I2C_HandleTypeDef *hi2c,
                        int32_t *adc_T, int32_t *adc_P, int32_t *adc_H);

/* 已知原始ADC值，直接补偿（不读I2C，避免竞争） */
uint8_t  BME280_Compensate(int32_t adc_T, int32_t adc_P, int32_t adc_H,
                           float *temperature, float *humidity, float *pressure);

#endif /* __BME280_H */
