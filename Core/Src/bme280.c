/**
 * bme280.c - 完整 BME280 驱动
 * - 软件 I2C (SoftI2C bit-bang)
 * - Bosch 官方 64 位整数补偿公�? * - 提供 BME280_Compensate() 支持先读后算（不重复�?I2C�? */

#include "bme280.h"
#include "soft_i2c.h"

/* 内部全局变量：保存校准参�?*/
static BME280_CalibData calib;
static int32_t t_fine;   /* 温度补偿中间值，供气�?湿度计算使用 */

#define I2C_ADDR  BME280_ADDR

/* ==================== I2C 底层 ==================== */
static uint8_t read_regs(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *buf, uint8_t len)
{
    (void)hi2c;
    return SoftI2C_ReadRegs(I2C_ADDR, reg, buf, len);
}

static uint8_t write_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t data)
{
    (void)hi2c;
    return SoftI2C_WriteReg(I2C_ADDR, reg, data);
}

/* ==================== 校准参数读取 ==================== */
static uint8_t read_calibration(I2C_HandleTypeDef *hi2c)
{
    uint8_t buf[26], hbuf[7];

    if (read_regs(hi2c, 0x88, buf, 26) != HAL_OK) return 1;

    calib.dig_T1 = (uint16_t)(buf[0] | (buf[1] << 8));
    calib.dig_T2 = (int16_t)(buf[2] | (buf[3] << 8));
    calib.dig_T3 = (int16_t)(buf[4] | (buf[5] << 8));
    calib.dig_P1 = (uint16_t)(buf[6] | (buf[7] << 8));
    calib.dig_P2 = (int16_t)(buf[8] | (buf[9] << 8));
    calib.dig_P3 = (int16_t)(buf[10] | (buf[11] << 8));
    calib.dig_P4 = (int16_t)(buf[12] | (buf[13] << 8));
    calib.dig_P5 = (int16_t)(buf[14] | (buf[15] << 8));
    calib.dig_P6 = (int16_t)(buf[16] | (buf[17] << 8));
    calib.dig_P7 = (int16_t)(buf[18] | (buf[19] << 8));
    calib.dig_P8 = (int16_t)(buf[20] | (buf[21] << 8));
    calib.dig_P9 = (int16_t)(buf[22] | (buf[23] << 8));
    calib.dig_H1 = buf[25];

    if (read_regs(hi2c, 0xE1, hbuf, 7) != HAL_OK) return 1;

    calib.dig_H2 = (int16_t)(hbuf[0] | (hbuf[1] << 8));
    calib.dig_H3 = hbuf[2];
    calib.dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    calib.dig_H5 = (int16_t)((hbuf[4] >> 4) | (hbuf[5] << 4));
    calib.dig_H6 = (int8_t)hbuf[6];

    return 0;
}

/* ==================== 公共 API ==================== */

uint8_t BME280_ReadID(I2C_HandleTypeDef *hi2c)
{
    uint8_t id;
    if (read_regs(hi2c, 0xD0, &id, 1) != HAL_OK) return 0xFF;
    return id;
}

void BME280_Reset(I2C_HandleTypeDef *hi2c)
{
    write_reg(hi2c, 0xE0, 0xB6);
}

uint8_t BME280_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = BME280_ReadID(hi2c);
    if (id == 0xFF) return 1;
    if (id != 0x60) return 2;

    BME280_Reset(hi2c);
    HAL_Delay(10);
    if (read_calibration(hi2c) != 0) return 1;

    write_reg(hi2c, 0xF2, 0x01);  /* ctrl_hum: osrs_h=x1 */
    write_reg(hi2c, 0xF5, 0x00);  /* config: t_sb=0, filter=0 */
    write_reg(hi2c, 0xF4, 0x27);  /* ctrl_meas: osrs_t=x1, osrs_p=x1, normal */
    HAL_Delay(20);
    return 0;
}

uint8_t BME280_ForceMeasure(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;
    return 0;
}

/* ==================== 补偿公式（核心）==================== */

/* 单次读取 + 补偿 */
static uint8_t compensate_one_read(I2C_HandleTypeDef *hi2c,
    float *temperature, float *humidity, float *pressure)
{
    uint8_t buf[8];
    int32_t adc_T, adc_P, adc_H;

    if (read_regs(hi2c, 0xF7, buf, 8) != HAL_OK) return 1;

    adc_P = (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
    adc_T = (int32_t)(((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4));
    adc_H = (int32_t)((uint16_t)(buf[6] << 8) | buf[7]);
    
    

    return BME280_Compensate(adc_T, adc_P, adc_H, temperature, humidity, pressure);
}

/* 读取温度（单独） */
float BME280_ReadTemperature(I2C_HandleTypeDef *hi2c)
{
    float t, h, p;
    compensate_one_read(hi2c, &t, &h, &p);
    return t;
}

/* 读取湿度（单独） */
float BME280_ReadHumidity(I2C_HandleTypeDef *hi2c)
{
    float t, h, p;
    compensate_one_read(hi2c, &t, &h, &p);
    return h;
}

/* 读取气压（单独） */
float BME280_ReadPressure(I2C_HandleTypeDef *hi2c)
{
    float t, h, p;
    compensate_one_read(hi2c, &t, &h, &p);
    return p;
}

/* 一次性读取全部（推荐�?*/
uint8_t BME280_ReadAll(I2C_HandleTypeDef *hi2c,
    float *temperature, float *humidity, float *pressure)
{
    return compensate_one_read(hi2c, temperature, humidity, pressure);
}

/* ==================== BME280_Compensate：给定ADC值算，不读I2C ==================== */
uint8_t BME280_Compensate(int32_t adc_T, int32_t adc_P, int32_t adc_H,
                           float *temperature, float *humidity, float *pressure)
{
    int32_t var1, var2, T;
    int64_t v1, v2, p;
    int32_t v_x1_u32r;

    /* ------- 温度 ------- */
    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    *temperature = T / 100.0f;

    /* ------- 气压 (依赖 t_fine) ------- */
    v1 = ((int64_t)t_fine) - 128000;
    v2 = v1 * v1 * (int64_t)calib.dig_P6;
    v2 = v2 + ((v1 * (int64_t)calib.dig_P5) << 17);
    v2 = v2 + (((int64_t)calib.dig_P4) << 35);
    v1 = ((v1 * v1 * (int64_t)calib.dig_P3) >> 8) + ((v1 * (int64_t)calib.dig_P2) << 12);
    v1 = (((((int64_t)1) << 47) + v1)) * ((int64_t)calib.dig_P1) >> 33;

    if (v1 != 0) {
        p = 1048576 - (int64_t)adc_P;
        p = (((p << 31) - v2) * 3125) / v1;
        v1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        v2 = (((int64_t)calib.dig_P8) * p) >> 19;
        p = ((p + v1 + v2) >> 8) + (((int64_t)calib.dig_P7) << 4);
        *pressure = p / 25600.0f;
    } else {
        *pressure = 0.0f;
    }

    /* ------- 湿度 (依赖 t_fine) ------- */
    v_x1_u32r = (t_fine - (int32_t)76800);
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.dig_H4) << 20) - (((int32_t)calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) + (int32_t)32768)) >> 10) + ((int32_t)2097152)) * ((int32_t)calib.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calib.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    *humidity = (float)(v_x1_u32r >> 12) / 1024.0f;

    return 0;
}

/* 调试：读取原始ADC�?*/
uint8_t BME280_ReadRaw(I2C_HandleTypeDef *hi2c, int32_t *adc_T, int32_t *adc_P, int32_t *adc_H)
{
    uint8_t buf[8];
    if (read_regs(hi2c, 0xF7, buf, 8) != HAL_OK) return 1;

    *adc_P = (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
    *adc_T = (int32_t)(((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4));
    *adc_H = (int32_t)((uint16_t)(buf[6] << 8) | buf[7]);
    
    
    return 0;
}
