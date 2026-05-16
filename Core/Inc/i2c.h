/**
  ******************************************************************************
  * @file           : i2c.h
  * @brief          : Header for i2c.c file (I2C1, PB6-SCL PB7-SDA)
  ******************************************************************************
  */
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

void MX_I2C1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */
