#include "ads1x1x.h"
#include <stddef.h>

/* Sadece 12-bit ailede (ADS1014/1015) gecerli sag-kaydirma miktari:
 * conversion register'da 12-bit sonuc, 16-bit alanin ust kisminda sol-hizali
 * durur. */
#define ADS1X1X_12BIT_RIGHT_SHIFT 4u

/* 12-bit ailede (ADS1014/1015) DR index -> SPS */
static const uint16_t ADS101X_DR_TABLE[8] = {128,  250,  490,  920,
                                             1600, 2400, 3300, 3300};

/* 16-bit ailede (ADS1115) DR index -> SPS */
static const uint16_t ADS111X_DR_TABLE[8] = {8, 16, 32, 64, 128, 250, 475, 860};

/* PGA index -> tam skala araligi (mV), driver icinde volt donusumunde
 * kullanilir */
static const float ADS1X1X_PGA_FSR_MV[6] = {6144.0f, 4096.0f, 2048.0f,
                                            1024.0f, 512.0f,  256.0f};

static bool ads1x1x_variant_has_mux(ads1x1x_variant_t variant) {
  /* ADS1014'te fiziksel MUX yok, sadece sabit AIN0-AIN1 diferansiyel giris var.
   */
  return (variant == ADS1X1X_VARIANT_ADS1015) ||
         (variant == ADS1X1X_VARIANT_ADS1115);
}

static bool ads1x1x_variant_is_16bit(ads1x1x_variant_t variant) {
  return variant == ADS1X1X_VARIANT_ADS1115;
}

ads1x1x_status_t ads1x1x_init(ads1x1x_t *dev, ads1x1x_variant_t variant,
                              uint8_t addr, ads1x1x_i2c_read_fn read_cb,
                              ads1x1x_i2c_write_fn write_cb) {
  if (dev == NULL || read_cb == NULL || write_cb == NULL) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }
  if (variant != ADS1X1X_VARIANT_ADS1014 &&
      variant != ADS1X1X_VARIANT_ADS1015 &&
      variant != ADS1X1X_VARIANT_ADS1115) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }

  dev->dev_addr = addr;
  dev->variant = variant;
  dev->read = read_cb;
  dev->write = write_cb;
  dev->config_shadow = ADS1X1X_CFG_RESET_VALUE;
  dev->is_initialized = true;

  return ADS1X1X_OK;
}

ads1x1x_status_t ads1x1x_write_config_raw(ads1x1x_t *dev, uint16_t config_val) {
  if (dev == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  uint8_t data[2];
  data[0] = (uint8_t)(config_val >> 8);    /* MSB */
  data[1] = (uint8_t)(config_val & 0xFFu); /* LSB */

  ads1x1x_status_t status =
      dev->write(dev->dev_addr, ADS1X1X_REG_CONFIG, data, 2);
  if (status == ADS1X1X_OK) {
    dev->config_shadow = config_val;
  }
  return status;
}

ads1x1x_status_t ads1x1x_read_config_raw(ads1x1x_t *dev, uint16_t *config_val) {
  if (dev == NULL || config_val == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  uint8_t buffer[2];
  ads1x1x_status_t status =
      dev->read(dev->dev_addr, ADS1X1X_REG_CONFIG, buffer, 2);
  if (status == ADS1X1X_OK) {
    *config_val = (uint16_t)((buffer[0] << 8) | buffer[1]);
    dev->config_shadow = *config_val;
  }
  return status;
}

ads1x1x_status_t ads1x1x_configure(ads1x1x_t *dev, ads1x1x_mux_t mux,
                                   ads1x1x_pga_t pga, ads1x1x_mode_t mode,
                                   ads1x1x_datarate_t data_rate) {
  if (dev == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  /* ADS1014'te MUX yok: kullanicinin default disinda bir kanal istemesi hata.
   */
  if (!ads1x1x_variant_has_mux(dev->variant) && mux != ADS1X1X_MUX_AIN0_AIN1) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }
  if (mux > ADS1X1X_MUX_AIN3_GND) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }
  if (pga > ADS1X1X_PGA_FSR_0256MV) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }
  if (data_rate > ADS1X1X_DR_IDX7) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }

  uint16_t config_val = 0;
  config_val |= (uint16_t)(1u << ADS1X1X_CFG_OS_BIT); /* yeni donusum baslat */
  config_val |= (uint16_t)((uint16_t)mux << ADS1X1X_CFG_MUX_SHIFT);
  config_val |= (uint16_t)((uint16_t)pga << ADS1X1X_CFG_PGA_SHIFT);
  config_val |= (uint16_t)((uint16_t)mode << ADS1X1X_CFG_MODE_BIT);
  config_val |= (uint16_t)((uint16_t)data_rate << ADS1X1X_CFG_DR_SHIFT);
  config_val |=
      (uint16_t)(ADS1X1X_CFG_COMP_QUE_DISABLE << ADS1X1X_CFG_COMP_QUE_SHIFT);

  return ads1x1x_write_config_raw(dev, config_val);
}

ads1x1x_status_t ads1x1x_start_conversion(ads1x1x_t *dev) {
  if (dev == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  uint16_t config_val =
      dev->config_shadow | (uint16_t)(1u << ADS1X1X_CFG_OS_BIT);
  return ads1x1x_write_config_raw(dev, config_val);
}

ads1x1x_status_t ads1x1x_is_conversion_ready(ads1x1x_t *dev, bool *ready) {
  if (dev == NULL || ready == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  uint16_t config_val = 0;
  ads1x1x_status_t status = ads1x1x_read_config_raw(dev, &config_val);
  if (status == ADS1X1X_OK) {
    *ready = (config_val & (uint16_t)(1u << ADS1X1X_CFG_OS_BIT)) != 0u;
  }
  return status;
}

ads1x1x_status_t ads1x1x_read_raw(ads1x1x_t *dev, int16_t *raw_value) {
  if (dev == NULL || raw_value == NULL || !dev->is_initialized) {
    return ADS1X1X_ERROR_NOT_INITIALIZED;
  }

  uint8_t buffer[2];
  ads1x1x_status_t status =
      dev->read(dev->dev_addr, ADS1X1X_REG_CONVERSION, buffer, 2);
  if (status != ADS1X1X_OK) {
    return status;
  }

  int16_t raw = (int16_t)((buffer[0] << 8) | buffer[1]);

  if (!ads1x1x_variant_is_16bit(dev->variant)) {
    /* ADS1014/1015: 12-bit sonuc ustte sol-hizali, sag kaydirip normalize et.
     */
    raw = (int16_t)(raw >> ADS1X1X_12BIT_RIGHT_SHIFT);
  }

  *raw_value = raw;
  return ADS1X1X_OK;
}

ads1x1x_status_t ads1x1x_raw_to_millivolts(const ads1x1x_t *dev,
                                           int16_t raw_value, ads1x1x_pga_t pga,
                                           float *millivolts) {
  if (dev == NULL || millivolts == NULL || pga > ADS1X1X_PGA_FSR_0256MV) {
    return ADS1X1X_ERROR_INVALID_PARAM;
  }

  float fsr_mv = ADS1X1X_PGA_FSR_MV[pga];
  /* Tam skala kodu: 16-bit ailede 2^15, 12-bit ailede 2^11 (normalize edilmis
   * raw icin). */
  float full_scale_code =
      ads1x1x_variant_is_16bit(dev->variant) ? 32768.0f : 2048.0f;

  *millivolts = ((float)raw_value / full_scale_code) * fsr_mv;
  return ADS1X1X_OK;
}

uint16_t ads1x1x_datarate_to_sps(ads1x1x_variant_t variant,
                                 ads1x1x_datarate_t data_rate) {
  if (data_rate > ADS1X1X_DR_IDX7) {
    return 0;
  }
  return ads1x1x_variant_is_16bit(variant) ? ADS111X_DR_TABLE[data_rate]
                                           : ADS101X_DR_TABLE[data_rate];
}
