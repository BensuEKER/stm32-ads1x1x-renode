/**
 * @file test_ads1x1x.c
 * @brief ads1x1x.c/.h driver'i icin unit testler.
 *
 * ONEMLI: Bu testler gercek donanima veya Renode'a bagimli DEGILDIR.
 * Driver zaten I2C read/write'i callback function pointer'lari uzerinden
 * aldigi icin (ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn), test icin
 * gercek STM32 HAL yerine mock_i2c.c'deki sahte I2C veriyolunu veriyoruz.
 * Bu dosya sadece normal 'gcc' ile, host bilgisayarda (WSL/Linux) derlenip
 * calisir -- ARM cross-compiler'a da ihtiyac yoktur.
 *
 * Diyagram (gorev metnindeki gibi):
 *   C Driver (ads1x1x.c)
 *     |
 *     +-- Mock I2C Read  (mock_i2c_read)
 *     +-- Mock I2C Write (mock_i2c_write)
 *     +-- Driver Logic   (bu dosyadaki testler)
 */
#include "ads1x1x.h"
#include "mock_i2c.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------------- */
/* Minik assert-tabanli test cercevesi (harici kutuphane gerekmez)        */
/* ---------------------------------------------------------------------- */
static int g_tests_run    = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  [FAIL] %s (line %d): %s\n", __func__, __LINE__, msg); \
            g_tests_failed++;                                               \
            return;                                                        \
        }                                                                   \
    } while (0)

#define RUN_TEST(fn)                  \
    do {                              \
        printf("RUN  %s\n", #fn);     \
        g_tests_run++;                \
        int failed_before = g_tests_failed; \
        fn();                         \
        if (g_tests_failed == failed_before) { \
            printf("  [OK]\n");        \
        }                             \
    } while (0)

/* Her testten once cagrilir: temiz bir sensor handle + sifirlanmis mock */
static void setup(ads1x1x_t *dev, ads1x1x_variant_t variant)
{
    mock_i2c_reset();
    ads1x1x_status_t st = ads1x1x_init(dev, variant, ADS1X1X_I2C_ADDR_GND,
                                        mock_i2c_read, mock_i2c_write);
    (void)st; /* setup fonksiyonlarinda TEST_ASSERT kullanmiyoruz, cagiran kontrol eder */
}

/* ---------------------------------------------------------------------- */
/* 1) Initialization testleri                                              */
/* ---------------------------------------------------------------------- */
static void test_init_null_dev_returns_invalid_param(void)
{
    ads1x1x_status_t st = ads1x1x_init(NULL, ADS1X1X_VARIANT_ADS1115,
                                        ADS1X1X_I2C_ADDR_GND, mock_i2c_read, mock_i2c_write);
    TEST_ASSERT(st == ADS1X1X_ERROR_INVALID_PARAM, "NULL dev handle kabul edilmemeli");
}

static void test_init_null_callbacks_returns_invalid_param(void)
{
    ads1x1x_t dev;
    ads1x1x_status_t st1 = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1115,
                                         ADS1X1X_I2C_ADDR_GND, NULL, mock_i2c_write);
    ads1x1x_status_t st2 = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1115,
                                         ADS1X1X_I2C_ADDR_GND, mock_i2c_read, NULL);
    TEST_ASSERT(st1 == ADS1X1X_ERROR_INVALID_PARAM, "NULL read callback kabul edilmemeli");
    TEST_ASSERT(st2 == ADS1X1X_ERROR_INVALID_PARAM, "NULL write callback kabul edilmemeli");
}

static void test_init_valid_sets_fields_correctly(void)
{
    ads1x1x_t dev;
    memset(&dev, 0xAA, sizeof(dev)); /* eski cop veriyle doldur, init temizlemeli */

    ads1x1x_status_t st = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1015,
                                        ADS1X1X_I2C_ADDR_VDD, mock_i2c_read, mock_i2c_write);

    TEST_ASSERT(st == ADS1X1X_OK, "gecerli parametrelerle init basarili olmali");
    TEST_ASSERT(dev.is_initialized == true, "init sonrasi is_initialized true olmali");
    TEST_ASSERT(dev.variant == ADS1X1X_VARIANT_ADS1015, "variant dogru saklanmali");
    TEST_ASSERT(dev.dev_addr == ADS1X1X_I2C_ADDR_VDD, "adres dogru saklanmali");
}

/* ---------------------------------------------------------------------- */
/* 2) Configuration testleri (mux/pga/mode/data_rate -> config register)   */
/* ---------------------------------------------------------------------- */
static void test_configure_ads1014_rejects_non_default_mux(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1014);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_GND,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);

    TEST_ASSERT(st == ADS1X1X_ERROR_INVALID_PARAM,
                "ADS1014'te varsayilan disi mux reddedilmeli (fiziksel MUX yok)");
}

static void test_configure_ads1014_accepts_default_mux(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1014);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);

    TEST_ASSERT(st == ADS1X1X_OK, "ADS1014'te varsayilan mux (AIN0-AIN1) kabul edilmeli");
}

static void test_configure_ads1115_accepts_any_valid_mux(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN3_GND,
                                             ADS1X1X_PGA_FSR_4096MV,
                                             ADS1X1X_MODE_SINGLE_SHOT, ADS1X1X_DR_IDX7);

    TEST_ASSERT(st == ADS1X1X_OK, "ADS1115'te 4 kanal MUX'un tumu kabul edilmeli");
}

static void test_configure_writes_correct_bit_fields(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN1_AIN3,
                                             ADS1X1X_PGA_FSR_1024MV,
                                             ADS1X1X_MODE_SINGLE_SHOT, ADS1X1X_DR_IDX3);
    TEST_ASSERT(st == ADS1X1X_OK, "configure basarili olmali");

    uint16_t written = g_mock_i2c.registers[ADS1X1X_REG_CONFIG];

    uint16_t expected_mux = (uint16_t)(ADS1X1X_MUX_AIN1_AIN3 << ADS1X1X_CFG_MUX_SHIFT);
    uint16_t expected_pga = (uint16_t)(ADS1X1X_PGA_FSR_1024MV << ADS1X1X_CFG_PGA_SHIFT);
    uint16_t expected_mode = (uint16_t)(ADS1X1X_MODE_SINGLE_SHOT << ADS1X1X_CFG_MODE_BIT);
    uint16_t expected_dr   = (uint16_t)(ADS1X1X_DR_IDX3 << ADS1X1X_CFG_DR_SHIFT);

    TEST_ASSERT((written & (0x7u << ADS1X1X_CFG_MUX_SHIFT)) == expected_mux,
                "MUX bit alani dogru yazilmali");
    TEST_ASSERT((written & (0x7u << ADS1X1X_CFG_PGA_SHIFT)) == expected_pga,
                "PGA bit alani dogru yazilmali");
    TEST_ASSERT((written & (0x1u << ADS1X1X_CFG_MODE_BIT)) == expected_mode,
                "MODE biti dogru yazilmali");
    TEST_ASSERT((written & (0x7u << ADS1X1X_CFG_DR_SHIFT)) == expected_dr,
                "DR bit alani dogru yazilmali");
}

static void test_configure_rejects_out_of_range_pga(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             (ads1x1x_pga_t)99,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);
    TEST_ASSERT(st == ADS1X1X_ERROR_INVALID_PARAM, "gecersiz PGA reddedilmeli");
}

static void test_configure_before_init_returns_not_initialized(void)
{
    ads1x1x_t dev;
    memset(&dev, 0, sizeof(dev)); /* is_initialized = false */

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);
    TEST_ASSERT(st == ADS1X1X_ERROR_NOT_INITIALIZED,
                "init edilmemis handle ile configure calismamali");
}

/* ---------------------------------------------------------------------- */
/* 3) read_raw testleri -- 16-bit vs 12-bit normalizasyonu                 */
/* ---------------------------------------------------------------------- */
static void test_read_raw_16bit_variant_returns_full_value(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);

    /* Mock'un conversion register'ina dogrudan bilinen bir deger koyuyoruz */
    g_mock_i2c.registers[ADS1X1X_REG_CONVERSION] = 0x1234u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT(st == ADS1X1X_OK, "read_raw basarili olmali");
    TEST_ASSERT(raw == (int16_t)0x1234, "16-bit variant'ta deger oldugu gibi donmeli");
}

static void test_read_raw_12bit_variant_right_aligns_value(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1015);

    /* Datasheet'e gore 12-bit sonuc registerin ustunde sol-hizali durur;
     * alt 4 bit her zaman 0'dir. Ornek: gercek 12-bit deger 0x123 ise,
     * register'da 0x1230 olarak durur. */
    g_mock_i2c.registers[ADS1X1X_REG_CONVERSION] = 0x1230u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT(st == ADS1X1X_OK, "read_raw basarili olmali");
    TEST_ASSERT(raw == 0x123, "12-bit variant'ta deger sag hizali (>>4) donmeli");
}

static void test_read_raw_negative_value_12bit_sign_extends_correctly(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1015);

    /* -1 (tum bitler 1) register'da 0xFFF0 olarak durur (alt 4 bit 0) */
    g_mock_i2c.registers[ADS1X1X_REG_CONVERSION] = 0xFFF0u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT(st == ADS1X1X_OK, "read_raw basarili olmali");
    TEST_ASSERT(raw == -1, "negatif 12-bit deger dogru isaretle (sign) genisletilmeli");
}

static void test_read_raw_comm_error_propagates(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);
    g_mock_i2c.force_comm_error = true;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT(st == ADS1X1X_ERROR_COMM, "I2C hatasi driver tarafindan iletilmeli");
}

/* ---------------------------------------------------------------------- */
/* 4) raw_to_millivolts testleri                                           */
/* ---------------------------------------------------------------------- */
static void test_raw_to_millivolts_16bit_full_scale(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1115);

    float mv = 0.0f;
    /* 16-bit tam skala kodu (32768) ve PGA=+-2048mV ile tam olcek degeri
     * 2048mV'a esit olmali (raw = full scale code -> fsr) */
    ads1x1x_status_t st = ads1x1x_raw_to_millivolts(&dev, 16384 /* yarim skala */,
                                                      ADS1X1X_PGA_FSR_2048MV, &mv);

    TEST_ASSERT(st == ADS1X1X_OK, "donusum basarili olmali");
    TEST_ASSERT(fabsf(mv - 1024.0f) < 1.0f, "yarim skala kod, PGA araliginin yarisina denk gelmeli");
}

static void test_raw_to_millivolts_12bit_uses_correct_scale(void)
{
    ads1x1x_t dev;
    setup(&dev, ADS1X1X_VARIANT_ADS1014);

    float mv = 0.0f;
    /* 12-bit tam skala kodu 2048; raw=1024 -> PGA araliginin yarisi */
    ads1x1x_status_t st = ads1x1x_raw_to_millivolts(&dev, 1024,
                                                      ADS1X1X_PGA_FSR_2048MV, &mv);

    TEST_ASSERT(st == ADS1X1X_OK, "donusum basarili olmali");
    TEST_ASSERT(fabsf(mv - 1024.0f) < 1.0f, "12-bit variant'ta da orantili sonuc beklenir");
}

/* ---------------------------------------------------------------------- */
/* main -- tum testleri calistirir, ozet basar                             */
/* ---------------------------------------------------------------------- */
int main(void)
{
    printf("=== ads1x1x driver unit testleri (mock I2C, Renode/donanim BAGIMLI DEGIL) ===\n\n");

    RUN_TEST(test_init_null_dev_returns_invalid_param);
    RUN_TEST(test_init_null_callbacks_returns_invalid_param);
    RUN_TEST(test_init_valid_sets_fields_correctly);

    RUN_TEST(test_configure_ads1014_rejects_non_default_mux);
    RUN_TEST(test_configure_ads1014_accepts_default_mux);
    RUN_TEST(test_configure_ads1115_accepts_any_valid_mux);
    RUN_TEST(test_configure_writes_correct_bit_fields);
    RUN_TEST(test_configure_rejects_out_of_range_pga);
    RUN_TEST(test_configure_before_init_returns_not_initialized);

    RUN_TEST(test_read_raw_16bit_variant_returns_full_value);
    RUN_TEST(test_read_raw_12bit_variant_right_aligns_value);
    RUN_TEST(test_read_raw_negative_value_12bit_sign_extends_correctly);
    RUN_TEST(test_read_raw_comm_error_propagates);

    RUN_TEST(test_raw_to_millivolts_16bit_full_scale);
    RUN_TEST(test_raw_to_millivolts_12bit_uses_correct_scale);

    printf("\n=== SONUC: %d testten %d basarisiz ===\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? 0 : 1;
}
