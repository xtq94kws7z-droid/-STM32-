/**
 * soft_i2c.h - 软件模拟 I2C (Bit-Bang) 声明
 * PB6 = SCL, PB7 = SDA
 */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

#define BME280_I2C_ADDR  0x76
#define OLED_I2C_ADDR    0x3C

/* 初始化PB6/PB7为开漏输出+上拉 */
void SoftI2C_Init(void);

/* 写寄存器 */
uint8_t SoftI2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t data);

/* 连续读 */
uint8_t SoftI2C_ReadRegs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len);

/* 连续写数据 (用于OLED: start + addr + ctrl + data...) */
uint8_t SoftI2C_WriteMulti(uint8_t dev_addr, uint8_t ctrl_byte,
                           const uint8_t *data, uint8_t len);

#endif /* __SOFT_I2C_H */
