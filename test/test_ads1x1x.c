/**
 * @file test_ads1x1x.c
 * @brief Ceedling/Unity unit tests for the ads1x1x driver.
 *
 * These tests do NOT depend on real hardware or Renode -- the driver
 * already takes its I2C read/write functions as callbacks
 * (ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn), so a hand-written fake
 * I2C bus (fake_i2c.h/.c, in test/support/) is enough to exercise it.
 *
 * Run with:
 *   ceedling test:all
 */
#include "unity.h"
#include "ads1x1x.h"
#include "fake_i2c.h"
#include <string.h>

void setUp(void)
{
    /* Runs before every single test function. */
}

void tearDown(void)
{
    /* Runs after every single test function. */
}

static void init_device(ads1x1x_t *dev, ads1x1x_variant_t variant)
{
    fake_i2c_reset();
    ads1x1x_init(dev, variant, ADS1X1X_I2C_ADDR_GND, fake_i2c_read, fake_i2c_write);
}

/* ---------------------------------------------------------------------- */
/* 1) Initialization                                                       */
/* ---------------------------------------------------------------------- */
void test_init_null_dev_returns_invalid_param(void)
{
    ads1x1x_status_t st = ads1x1x_init(NULL, ADS1X1X_VARIANT_ADS1115,
                                        ADS1X1X_I2C_ADDR_GND, fake_i2c_read, fake_i2c_write);
    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_INVALID_PARAM, st);
}

void test_init_null_callbacks_returns_invalid_param(void)
{
    ads1x1x_t dev;
    ads1x1x_status_t st1 = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1115,
                                         ADS1X1X_I2C_ADDR_GND, NULL, fake_i2c_write);
    ads1x1x_status_t st2 = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1115,
                                         ADS1X1X_I2C_ADDR_GND, fake_i2c_read, NULL);
    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_INVALID_PARAM, st1);
    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_INVALID_PARAM, st2);
}

void test_init_valid_sets_fields_correctly(void)
{
    ads1x1x_t dev;
    memset(&dev, 0xAA, sizeof(dev));

    ads1x1x_status_t st = ads1x1x_init(&dev, ADS1X1X_VARIANT_ADS1015,
                                        ADS1X1X_I2C_ADDR_VDD, fake_i2c_read, fake_i2c_write);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_TRUE(dev.is_initialized);
    TEST_ASSERT_EQUAL(ADS1X1X_VARIANT_ADS1015, dev.variant);
    TEST_ASSERT_EQUAL_HEX8(ADS1X1X_I2C_ADDR_VDD, dev.dev_addr);
}

/* ---------------------------------------------------------------------- */
/* 2) Configuration                                                         */
/* ---------------------------------------------------------------------- */
void test_configure_ads1014_rejects_non_default_mux(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1014);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_GND,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);

    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_INVALID_PARAM, st);
}

void test_configure_ads1014_accepts_default_mux(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1014);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
}

void test_configure_ads1115_accepts_any_valid_mux(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN3_GND,
                                             ADS1X1X_PGA_FSR_4096MV,
                                             ADS1X1X_MODE_SINGLE_SHOT, ADS1X1X_DR_IDX7);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
}

void test_configure_writes_correct_bit_fields(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN1_AIN3,
                                             ADS1X1X_PGA_FSR_1024MV,
                                             ADS1X1X_MODE_SINGLE_SHOT, ADS1X1X_DR_IDX3);
    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);

    uint16_t written = g_fake_i2c.registers[ADS1X1X_REG_CONFIG];

    uint16_t expected_mux  = (uint16_t)(ADS1X1X_MUX_AIN1_AIN3 << ADS1X1X_CFG_MUX_SHIFT);
    uint16_t expected_pga  = (uint16_t)(ADS1X1X_PGA_FSR_1024MV << ADS1X1X_CFG_PGA_SHIFT);
    uint16_t expected_mode = (uint16_t)(ADS1X1X_MODE_SINGLE_SHOT << ADS1X1X_CFG_MODE_BIT);
    uint16_t expected_dr   = (uint16_t)(ADS1X1X_DR_IDX3 << ADS1X1X_CFG_DR_SHIFT);

    TEST_ASSERT_EQUAL_HEX16(expected_mux,  written & (0x7u << ADS1X1X_CFG_MUX_SHIFT));
    TEST_ASSERT_EQUAL_HEX16(expected_pga,  written & (0x7u << ADS1X1X_CFG_PGA_SHIFT));
    TEST_ASSERT_EQUAL_HEX16(expected_mode, written & (0x1u << ADS1X1X_CFG_MODE_BIT));
    TEST_ASSERT_EQUAL_HEX16(expected_dr,   written & (0x7u << ADS1X1X_CFG_DR_SHIFT));
}

void test_configure_rejects_out_of_range_pga(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             (ads1x1x_pga_t)99,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);
    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_INVALID_PARAM, st);
}

void test_configure_before_init_returns_not_initialized(void)
{
    ads1x1x_t dev;
    memset(&dev, 0, sizeof(dev));

    ads1x1x_status_t st = ads1x1x_configure(&dev, ADS1X1X_MUX_AIN0_AIN1,
                                             ADS1X1X_PGA_FSR_2048MV,
                                             ADS1X1X_MODE_CONTINUOUS, ADS1X1X_DR_IDX4);
    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_NOT_INITIALIZED, st);
}

/* ---------------------------------------------------------------------- */
/* 3) read_raw -- 16-bit vs 12-bit normalization                           */
/* ---------------------------------------------------------------------- */
void test_read_raw_16bit_variant_returns_full_value(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);
    g_fake_i2c.registers[ADS1X1X_REG_CONVERSION] = 0x1234u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_EQUAL_INT16((int16_t)0x1234, raw);
}

void test_read_raw_12bit_variant_right_aligns_value(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1015);
    g_fake_i2c.registers[ADS1X1X_REG_CONVERSION] = 0x1230u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_EQUAL_INT16(0x123, raw);
}

void test_read_raw_negative_value_12bit_sign_extends_correctly(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1015);
    g_fake_i2c.registers[ADS1X1X_REG_CONVERSION] = 0xFFF0u;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_EQUAL_INT16(-1, raw);
}

void test_read_raw_comm_error_propagates(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);
    g_fake_i2c.force_comm_error = true;

    int16_t raw = 0;
    ads1x1x_status_t st = ads1x1x_read_raw(&dev, &raw);

    TEST_ASSERT_EQUAL(ADS1X1X_ERROR_COMM, st);
}

/* ---------------------------------------------------------------------- */
/* 4) raw_to_millivolts                                                     */
/* ---------------------------------------------------------------------- */
void test_raw_to_millivolts_16bit_full_scale(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1115);

    float mv = 0.0f;
    ads1x1x_status_t st = ads1x1x_raw_to_millivolts(&dev, 16384,
                                                      ADS1X1X_PGA_FSR_2048MV, &mv);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1024.0f, mv);
}

void test_raw_to_millivolts_12bit_uses_correct_scale(void)
{
    ads1x1x_t dev;
    init_device(&dev, ADS1X1X_VARIANT_ADS1014);

    float mv = 0.0f;
    ads1x1x_status_t st = ads1x1x_raw_to_millivolts(&dev, 1024,
                                                      ADS1X1X_PGA_FSR_2048MV, &mv);

    TEST_ASSERT_EQUAL(ADS1X1X_OK, st);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1024.0f, mv);
}
