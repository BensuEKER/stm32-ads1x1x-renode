#ifndef INC_ADS1X1X_H_
#define INC_ADS1X1X_H_

/**
 * @file    ads1x1x.h
 * @brief   Portable C driver for the ADS1014 / ADS1015 / ADS1115 family
 *          (Texas Instruments I2C analog-to-digital converters).
 *
 * A single library supports all three chips:
 *   - ADS1014 : 12-bit, single (fixed) differential input, no MUX
 *   - ADS1015 : 12-bit, 4-channel MUX (single-ended / differential)
 *   - ADS1115 : 16-bit, 4-channel MUX (single-ended / differential)
 *
 * The register map is identical across all three (Conversion/Config/
 * Lo_thresh/Hi_thresh). The differences are resolution (12-bit vs 16-bit)
 * and whether MUX is present. This driver is hardware-independent; I2C
 * read/write operations go through caller-supplied callback functions
 * (see ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn), so it is not tied
 * directly to STM32 HAL/LL or any other platform.
 */

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* I2C Addresses (depend on where the ADDR pin is tied; valid for all three) */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_I2C_ADDR_GND 0x48u /* ADDR -> GND (default) */
#define ADS1X1X_I2C_ADDR_VDD 0x49u /* ADDR -> VDD */
#define ADS1X1X_I2C_ADDR_SDA 0x4Au /* ADDR -> SDA */
#define ADS1X1X_I2C_ADDR_SCL 0x4Bu /* ADDR -> SCL */

/* ------------------------------------------------------------------------ */
/* Register Addresses (common to ADS1014 / ADS1015 / ADS1115)              */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_REG_CONVERSION 0x00u
#define ADS1X1X_REG_CONFIG 0x01u
#define ADS1X1X_REG_LO_THRESH 0x02u
#define ADS1X1X_REG_HI_THRESH 0x03u

/* ------------------------------------------------------------------------ */
/* Config Register Bit Fields (16-bit, MSB written first)                  */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_CFG_OS_BIT                                                                         \
    15u /* write: 1 = start single-shot conversion / read: 1 = conversion done */
#define ADS1X1X_CFG_MUX_SHIFT 12u /* bits 14:12 */
#define ADS1X1X_CFG_PGA_SHIFT 9u  /* bits 11:9  */
#define ADS1X1X_CFG_MODE_BIT 8u   /* bit 8: 0 = continuous, 1 = single-shot */
#define ADS1X1X_CFG_DR_SHIFT 5u   /* bits 7:5   */
#define ADS1X1X_CFG_COMP_MODE_BIT 4u
#define ADS1X1X_CFG_COMP_POL_BIT 3u
#define ADS1X1X_CFG_COMP_LAT_BIT 2u
#define ADS1X1X_CFG_COMP_QUE_SHIFT 0u /* bits 1:0   */
#define ADS1X1X_CFG_COMP_QUE_DISABLE                                                               \
    0x3u /* comparator disabled (default)                                                          \
          */

/* Reset/default config value (per datasheet): 0x8583
 * (OS=1, MUX=AIN0-AIN1, PGA=+-2.048V, MODE=single-shot, DR=mid rate, COMP
 * disabled) */
#define ADS1X1X_CFG_RESET_VALUE 0x8583u

/* ------------------------------------------------------------------------ */
/* Status Codes (Error Handling)                                            */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_OK = 0,
    ADS1X1X_ERROR_INVALID_PARAM,  /* NULL pointer, unsupported mux/pga/dr, etc.
                                   */
    ADS1X1X_ERROR_COMM,           /* I2C read/write failed */
    ADS1X1X_ERROR_NOT_INITIALIZED /* used before init was called */
} ads1x1x_status_t;

/* ------------------------------------------------------------------------ */
/* Supported Chip Variants                                                  */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_VARIANT_ADS1014, /* 12-bit, no MUX (fixed differential AIN0-AIN1) */
    ADS1X1X_VARIANT_ADS1015, /* 12-bit, 4-channel MUX */
    ADS1X1X_VARIANT_ADS1115  /* 16-bit, 4-channel MUX */
} ads1x1x_variant_t;

/* ------------------------------------------------------------------------ */
/* MUX (Channel) Selection -- only applicable to ADS1015 / ADS1115         */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_MUX_AIN0_AIN1 = 0, /* differential, the ONLY mode ADS1014 supports */
    ADS1X1X_MUX_AIN0_AIN3 = 1,
    ADS1X1X_MUX_AIN1_AIN3 = 2,
    ADS1X1X_MUX_AIN2_AIN3 = 3,
    ADS1X1X_MUX_AIN0_GND  = 4, /* single-ended */
    ADS1X1X_MUX_AIN1_GND  = 5,
    ADS1X1X_MUX_AIN2_GND  = 6,
    ADS1X1X_MUX_AIN3_GND  = 7
} ads1x1x_mux_t;

/* ------------------------------------------------------------------------ */
/* PGA (Gain / Full-Scale Range) -- same bit field across all three         */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_PGA_FSR_6144MV = 0, /* +-6.144V */
    ADS1X1X_PGA_FSR_4096MV = 1, /* +-4.096V */
    ADS1X1X_PGA_FSR_2048MV = 2, /* +-2.048V (POR default) */
    ADS1X1X_PGA_FSR_1024MV = 3, /* +-1.024V */
    ADS1X1X_PGA_FSR_0512MV = 4, /* +-0.512V */
    ADS1X1X_PGA_FSR_0256MV = 5  /* +-0.256V (same as 6 and 7) */
} ads1x1x_pga_t;

/* ------------------------------------------------------------------------ */
/* Operating Mode                                                           */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_MODE_CONTINUOUS  = 0,
    ADS1X1X_MODE_SINGLE_SHOT = 1
} ads1x1x_mode_t;

/* ------------------------------------------------------------------------ */
/* Data Rate -- a 3-bit field, but the SPS meaning DIFFERS by family:      *
 *   ADS1014/1015 (12-bit family) : 128..3300 SPS                          *
 *   ADS1115      (16-bit family) : 8..860  SPS                            *
 * The index is shared; the actual SPS value is computed per variant via  *
 * ads1x1x_datarate_to_sps().                                              */
/* ------------------------------------------------------------------------ */
typedef enum
{
    ADS1X1X_DR_IDX0 = 0,
    ADS1X1X_DR_IDX1 = 1,
    ADS1X1X_DR_IDX2 = 2,
    ADS1X1X_DR_IDX3 = 3,
    ADS1X1X_DR_IDX4 = 4, /* POR default (1600SPS on 12-bit family, 128SPS on 16-bit family) */
    ADS1X1X_DR_IDX5 = 5,
    ADS1X1X_DR_IDX6 = 6,
    ADS1X1X_DR_IDX7 = 7
} ads1x1x_datarate_t;

/* ------------------------------------------------------------------------ */
/* Hardware Abstraction Layer -- I2C callback types                         */
/* ------------------------------------------------------------------------ */
typedef ads1x1x_status_t (*ads1x1x_i2c_read_fn)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                                                uint16_t len);
typedef ads1x1x_status_t (*ads1x1x_i2c_write_fn)(uint8_t dev_addr, uint8_t reg_addr,
                                                 const uint8_t *data, uint16_t len);

/* ------------------------------------------------------------------------ */
/* Sensor Handle (Context)                                                  */
/* ------------------------------------------------------------------------ */
typedef struct
{
    uint8_t dev_addr;
    ads1x1x_variant_t variant;
    ads1x1x_i2c_read_fn read;
    ads1x1x_i2c_write_fn write;
    uint16_t config_shadow; /* last known value of the config register */
    bool is_initialized;
} ads1x1x_t;

/* ------------------------------------------------------------------------ */
/* API                                                                       */
/* ------------------------------------------------------------------------ */

/** Initializes the sensor; stores the variant, address, and I2C callbacks. */
ads1x1x_status_t ads1x1x_init(ads1x1x_t *dev, ads1x1x_variant_t variant, uint8_t addr,
                              ads1x1x_i2c_read_fn read_cb, ads1x1x_i2c_write_fn write_cb);

/** Builds and writes the config register from meaningful fields
 * (mux/pga/mode/data_rate). For ADS1014, requesting mux !=
 * ADS1X1X_MUX_AIN0_AIN1 returns ADS1X1X_ERROR_INVALID_PARAM. */
ads1x1x_status_t ads1x1x_configure(ads1x1x_t *dev, ads1x1x_mux_t mux, ads1x1x_pga_t pga,
                                   ads1x1x_mode_t mode, ads1x1x_datarate_t data_rate);

/** Writes a raw 16-bit value to the config register (low-level access /
 * testing). */
ads1x1x_status_t ads1x1x_write_config_raw(ads1x1x_t *dev, uint16_t config_val);

/** Reads the config register. */
ads1x1x_status_t ads1x1x_read_config_raw(ads1x1x_t *dev, uint16_t *config_val);

/** Starts a new conversion in single-shot mode (sets the OS bit). */
ads1x1x_status_t ads1x1x_start_conversion(ads1x1x_t *dev);

/** Checks whether the conversion has finished (OS bit = 1 -> ready). */
ads1x1x_status_t ads1x1x_is_conversion_ready(ads1x1x_t *dev, bool *ready);

/** Reads the conversion register; normalizes the result based on the
 *  variant's resolution (12-bit variants return a right-aligned 12-bit
 *  value). */
ads1x1x_status_t ads1x1x_read_raw(ads1x1x_t *dev, int16_t *raw_value);

/** Converts a raw value to millivolts based on the given PGA range and the
 * variant's resolution. */
ads1x1x_status_t ads1x1x_raw_to_millivolts(const ads1x1x_t *dev, int16_t raw_value,
                                           ads1x1x_pga_t pga, float *millivolts);

/** The actual SPS value that the given data_rate index maps to for the
 * active variant. */
uint16_t ads1x1x_datarate_to_sps(ads1x1x_variant_t variant, ads1x1x_datarate_t data_rate);

#endif /* INC_ADS1X1X_H_ */
