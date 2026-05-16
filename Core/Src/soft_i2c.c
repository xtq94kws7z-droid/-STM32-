/**
 * soft_i2c.c - 软件模拟 I2C (Bit-Bang) 实现
 * PB6 = SCL, PB7 = SDA
 * 不受 STM32F1 硬件 I2C Bug 影响
 */

#include "soft_i2c.h"

#define SCL_PIN     GPIO_PIN_6
#define SDA_PIN     GPIO_PIN_7
#define PORT        GPIOB

static void delay_us(void)
{
    volatile uint32_t i;
    for (i = 0; i < 5; i++);  /* ~100kHz @ 8MHz */
}

void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = SCL_PIN | SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(PORT, SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PORT, SDA_PIN, GPIO_PIN_SET);
    delay_us();
}

/* ========== 底层时序 ========== */

static void SCL_H(void) { HAL_GPIO_WritePin(PORT, SCL_PIN, GPIO_PIN_SET); delay_us(); }
static void SCL_L(void) { HAL_GPIO_WritePin(PORT, SCL_PIN, GPIO_PIN_RESET); delay_us(); }
static void SDA_H(void) { HAL_GPIO_WritePin(PORT, SDA_PIN, GPIO_PIN_SET); delay_us(); }
static void SDA_L(void) { HAL_GPIO_WritePin(PORT, SDA_PIN, GPIO_PIN_RESET); delay_us(); }
static uint8_t SDA_R(void) { return HAL_GPIO_ReadPin(PORT, SDA_PIN); }

static void i2c_start(void)
{
    SDA_H(); SCL_H(); SDA_L(); SCL_L();
}

static void i2c_stop(void)
{
    SDA_L(); SCL_H(); delay_us(); SDA_H();
}

static uint8_t i2c_write_byte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (data & 0x80) SDA_H(); else SDA_L();
        data <<= 1;
        SCL_H(); SCL_L();
    }
    SDA_H();  /* 释放SDA等ACK */
    delay_us();
    SCL_H(); delay_us();
    uint8_t ack = SDA_R() ? 1 : 0;
    SCL_L();
    return ack;
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t data = 0, i;
    SDA_H();
    for (i = 0; i < 8; i++)
    {
        data <<= 1;
        SCL_H(); delay_us();
        if (SDA_R()) data |= 1;
        SCL_L();
    }
    if (ack) SDA_H(); else SDA_L();
    delay_us(); SCL_H(); delay_us(); SCL_L();
    SDA_H();
    return data;
}

/* ========== 对外接口 ========== */

uint8_t SoftI2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    i2c_start();
    if (i2c_write_byte(dev_addr << 1)) { i2c_stop(); return 1; }
    if (i2c_write_byte(reg)) { i2c_stop(); return 1; }
    if (i2c_write_byte(data)) { i2c_stop(); return 1; }
    i2c_stop();
    return 0;
}

uint8_t SoftI2C_ReadRegs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    i2c_start();
    if (i2c_write_byte(dev_addr << 1)) { i2c_stop(); return 1; }
    if (i2c_write_byte(reg)) { i2c_stop(); return 1; }
    i2c_stop();

    i2c_start();
    if (i2c_write_byte((dev_addr << 1) | 1)) { i2c_stop(); return 1; }
    for (i = 0; i < len; i++)
        buf[i] = i2c_read_byte(i == len - 1);
    i2c_stop();
    return 0;
}

uint8_t SoftI2C_WriteMulti(uint8_t dev_addr, uint8_t ctrl_byte,
                           const uint8_t *data, uint8_t len)
{
    uint8_t i;
    i2c_start();
    if (i2c_write_byte(dev_addr << 1)) { i2c_stop(); return 1; }
    if (i2c_write_byte(ctrl_byte)) { i2c_stop(); return 1; }
    for (i = 0; i < len; i++)
        if (i2c_write_byte(data[i])) { i2c_stop(); return 1; }
    i2c_stop();
    return 0;
}
