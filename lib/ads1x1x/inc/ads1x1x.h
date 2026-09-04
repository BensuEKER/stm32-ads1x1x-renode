#ifndef INC_ADS1X1X_H_
#define INC_ADS1X1X_H_

/**
 * @file    ads1x1x.h
 * @brief   Portable C driver for the ADS1014 / ADS1015 / ADS1115 family
 *          (Texas Instruments I2C analog-to-digital converters).
 *
 * Tek kutuphane, uc chip'i de destekler:
 *   - ADS1014 : 12-bit, tek (sabit) diferansiyel giris, MUX yok
 *   - ADS1015 : 12-bit, 4 kanal MUX (tek uclu / diferansiyel)
 *   - ADS1115 : 16-bit, 4 kanal MUX (tek uclu / diferansiyel)
 *
 * Register haritasi ucunde de aynidir (Conversion/Config/Lo_thresh/Hi_thresh).
 * Farklar: cozunurluk (12-bit vs 16-bit) ve MUX'un var/yok olusu.
 * Bu driver donanimdan bagimsizdir; I2C okuma/yazma islemleri kullanicinin
 * saglayacagi callback fonksiyonlari uzerinden yapilir (bkz.
 * ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn), boylece STM32 HAL/LL veya baska
 * bir platforma dogrudan bagli degildir.
 */

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* I2C Adresleri (ADDR pininin baglandigi yere gore, ucu icin de gecerli)   */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_I2C_ADDR_GND 0x48u /* ADDR -> GND (varsayilan/default) */
#define ADS1X1X_I2C_ADDR_VDD 0x49u /* ADDR -> VDD */
#define ADS1X1X_I2C_ADDR_SDA 0x4Au /* ADDR -> SDA */
#define ADS1X1X_I2C_ADDR_SCL 0x4Bu /* ADDR -> SCL */

/* ------------------------------------------------------------------------ */
/* Register Adresleri (ADS1014 / ADS1015 / ADS1115 ortak)                  */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_REG_CONVERSION 0x00u
#define ADS1X1X_REG_CONFIG 0x01u
#define ADS1X1X_REG_LO_THRESH 0x02u
#define ADS1X1X_REG_HI_THRESH 0x03u

/* ------------------------------------------------------------------------ */
/* Config Register Bit Alanlari (16-bit, MSB once yazilir)                 */
/* ------------------------------------------------------------------------ */
#define ADS1X1X_CFG_OS_BIT                                                     \
  15u /* 1 = single-shot baslat / 1 = conversion bitti (okunurken) */
#define ADS1X1X_CFG_MUX_SHIFT 12u /* bit 14:12 */
#define ADS1X1X_CFG_PGA_SHIFT 9u  /* bit 11:9  */
#define ADS1X1X_CFG_MODE_BIT 8u   /* bit 8: 0 = continuous, 1 = single-shot */
#define ADS1X1X_CFG_DR_SHIFT 5u   /* bit 7:5   */
#define ADS1X1X_CFG_COMP_MODE_BIT 4u
#define ADS1X1X_CFG_COMP_POL_BIT 3u
#define ADS1X1X_CFG_COMP_LAT_BIT 2u
#define ADS1X1X_CFG_COMP_QUE_SHIFT 0u     /* bit 1:0   */
#define ADS1X1X_CFG_COMP_QUE_DISABLE 0x3u /* comparator devre disi (default)   \
                                           */

/* Reset/default config degeri (datasheet): 0x8583
 * (OS=1, MUX=AIN0-AIN1, PGA=+-2.048V, MODE=single-shot, DR=orta hiz, COMP
 * disabled) */
#define ADS1X1X_CFG_RESET_VALUE 0x8583u

/* ------------------------------------------------------------------------ */
/* Durum Kodlari (Hata Yonetimi)                                            */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_OK = 0,
  ADS1X1X_ERROR_INVALID_PARAM,  /* NULL pointer, desteklenmeyen mux/pga/dr, vb.
                                 */
  ADS1X1X_ERROR_COMM,           /* I2C read/write basarisiz */
  ADS1X1X_ERROR_NOT_INITIALIZED /* init cagrilmadan kullanim */
} ads1x1x_status_t;

/* ------------------------------------------------------------------------ */
/* Desteklenen Chip Varyantlari                                             */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_VARIANT_ADS1014, /* 12-bit, MUX yok (sabit diferansiyel AIN0-AIN1) */
  ADS1X1X_VARIANT_ADS1015, /* 12-bit, 4 kanal MUX */
  ADS1X1X_VARIANT_ADS1115  /* 16-bit, 4 kanal MUX */
} ads1x1x_variant_t;

/* ------------------------------------------------------------------------ */
/* MUX (Kanal) Secimi -- sadece ADS1015 / ADS1115 icin gecerlidir           */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_MUX_AIN0_AIN1 = 0, /* diferansiyel, ADS1014'un TEK destekledigi mod */
  ADS1X1X_MUX_AIN0_AIN3 = 1,
  ADS1X1X_MUX_AIN1_AIN3 = 2,
  ADS1X1X_MUX_AIN2_AIN3 = 3,
  ADS1X1X_MUX_AIN0_GND = 4, /* tek uclu */
  ADS1X1X_MUX_AIN1_GND = 5,
  ADS1X1X_MUX_AIN2_GND = 6,
  ADS1X1X_MUX_AIN3_GND = 7
} ads1x1x_mux_t;

/* ------------------------------------------------------------------------ */
/* PGA (Kazanc / Tam Skala Araligi) -- ucunde de ayni bit alani             */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_PGA_FSR_6144MV = 0, /* +-6.144V */
  ADS1X1X_PGA_FSR_4096MV = 1, /* +-4.096V */
  ADS1X1X_PGA_FSR_2048MV = 2, /* +-2.048V (POR default) */
  ADS1X1X_PGA_FSR_1024MV = 3, /* +-1.024V */
  ADS1X1X_PGA_FSR_0512MV = 4, /* +-0.512V */
  ADS1X1X_PGA_FSR_0256MV = 5  /* +-0.256V (6 ve 7 ile ayni) */
} ads1x1x_pga_t;

/* ------------------------------------------------------------------------ */
/* Calisma Modu                                                             */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_MODE_CONTINUOUS = 0,
  ADS1X1X_MODE_SINGLE_SHOT = 1
} ads1x1x_mode_t;

/* ------------------------------------------------------------------------ */
/* Data Rate -- 3 bit alan, ama SPS karsiligi aileye gore FARKLIDIR:        *
 *   ADS1014/1015 (12-bit ailesi) : 128..3300 SPS                          *
 *   ADS1115      (16-bit ailesi) : 8..860  SPS                            *
 * Bu yuzden index ortak, gercek SPS degeri ads1x1x_datarate_to_sps() ile  *
 * variant'a gore hesaplanir.                                              */
/* ------------------------------------------------------------------------ */
typedef enum {
  ADS1X1X_DR_IDX0 = 0,
  ADS1X1X_DR_IDX1 = 1,
  ADS1X1X_DR_IDX2 = 2,
  ADS1X1X_DR_IDX3 = 3,
  ADS1X1X_DR_IDX4 =
      4, /* POR default (12-bit ailede 1600SPS, 16-bit ailede 128SPS) */
  ADS1X1X_DR_IDX5 = 5,
  ADS1X1X_DR_IDX6 = 6,
  ADS1X1X_DR_IDX7 = 7
} ads1x1x_datarate_t;

/* ------------------------------------------------------------------------ */
/* Hardware Abstraction Layer -- I2C callback tipleri                       */
/* ------------------------------------------------------------------------ */
typedef ads1x1x_status_t (*ads1x1x_i2c_read_fn)(uint8_t dev_addr,
                                                uint8_t reg_addr, uint8_t *data,
                                                uint16_t len);
typedef ads1x1x_status_t (*ads1x1x_i2c_write_fn)(uint8_t dev_addr,
                                                 uint8_t reg_addr,
                                                 const uint8_t *data,
                                                 uint16_t len);

/* ------------------------------------------------------------------------ */
/* Sensor Yapisi (Context / Handle)                                         */
/* ------------------------------------------------------------------------ */
typedef struct {
  uint8_t dev_addr;
  ads1x1x_variant_t variant;
  ads1x1x_i2c_read_fn read;
  ads1x1x_i2c_write_fn write;
  uint16_t config_shadow; /* config register'in son bilinen degeri */
  bool is_initialized;
} ads1x1x_t;

/* ------------------------------------------------------------------------ */
/* API                                                                       */
/* ------------------------------------------------------------------------ */

/** Sensoru baslatir; variant, adres ve I2C callback'lerini kaydeder. */
ads1x1x_status_t ads1x1x_init(ads1x1x_t *dev, ads1x1x_variant_t variant,
                              uint8_t addr, ads1x1x_i2c_read_fn read_cb,
                              ads1x1x_i2c_write_fn write_cb);

/** Config register'ini anlamli alanlardan (mux/pga/mode/data_rate) uretip
 * yazar. ADS1014 icin mux != ADS1X1X_MUX_AIN0_AIN1 istenirse
 * ADS1X1X_ERROR_INVALID_PARAM doner. */
ads1x1x_status_t ads1x1x_configure(ads1x1x_t *dev, ads1x1x_mux_t mux,
                                   ads1x1x_pga_t pga, ads1x1x_mode_t mode,
                                   ads1x1x_datarate_t data_rate);

/** Config register'ina ham 16-bit deger yazar (dusuk seviye erisim / test
 * icin). */
ads1x1x_status_t ads1x1x_write_config_raw(ads1x1x_t *dev, uint16_t config_val);

/** Config register'ini okur. */
ads1x1x_status_t ads1x1x_read_config_raw(ads1x1x_t *dev, uint16_t *config_val);

/** Single-shot modda yeni bir donusum baslatir (OS bitini set eder). */
ads1x1x_status_t ads1x1x_start_conversion(ads1x1x_t *dev);

/** Donusumun tamamlanip tamamlanmadigini kontrol eder (OS bit = 1 -> hazir). */
ads1x1x_status_t ads1x1x_is_conversion_ready(ads1x1x_t *dev, bool *ready);

/** Conversion register'ini okur; variant'in cozunurlugune gore normalize eder
 *  (12-bit varyantlarda sonuc sag hizali 12-bit deger olarak donulur). */
ads1x1x_status_t ads1x1x_read_raw(ads1x1x_t *dev, int16_t *raw_value);

/** Ham degeri, verilen PGA araligina ve variant'in cozunurlugune gore milivolta
 * cevirir. */
ads1x1x_status_t ads1x1x_raw_to_millivolts(const ads1x1x_t *dev,
                                           int16_t raw_value, ads1x1x_pga_t pga,
                                           float *millivolts);

/** Verilen data_rate index'inin, aktif variant icin karsilik geldigi gercek SPS
 * degeri. */
uint16_t ads1x1x_datarate_to_sps(ads1x1x_variant_t variant,
                                 ads1x1x_datarate_t data_rate);

#endif /* INC_ADS1X1X_H_ */
