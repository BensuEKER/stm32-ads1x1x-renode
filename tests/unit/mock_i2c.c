/**
 * @file mock_i2c.c
 * @brief mock_i2c.h implementasyonu.
 *
 * NOT: ads1x1x.c driver'i, register adresini HER cagrida ayri bir parametre
 * olarak gonderiyor (dev->read(dev_addr, reg_addr, data, len) / dev->write(...)) --
 * yani gercek I2C hattindaki "once pointer register'i yaz, sonra oku" adimini
 * driver'in STM32-ozel wrapper'i (main.c) hallediyor. Bu yuzden mock'un
 * ayri bir "pointer register" durumu tutmasina gerek yok; sadece verilen
 * reg_addr'a gore dogrudan registers[] dizisini okuyup yaziyoruz.
 */
#include "mock_i2c.h"
#include <string.h>

mock_i2c_state_t g_mock_i2c;

void mock_i2c_reset(void)
{
    memset(&g_mock_i2c, 0, sizeof(g_mock_i2c));
    /* Datasheet reset degerleri (ads1x1x.h ile birebir ayni) */
    g_mock_i2c.registers[ADS1X1X_REG_CONVERSION] = 0x0000u;
    g_mock_i2c.registers[ADS1X1X_REG_CONFIG]     = ADS1X1X_CFG_RESET_VALUE;
    g_mock_i2c.registers[ADS1X1X_REG_LO_THRESH]  = 0x8000u;
    g_mock_i2c.registers[ADS1X1X_REG_HI_THRESH]  = 0x7FFFu;
}

ads1x1x_status_t mock_i2c_write(uint8_t dev_addr, uint8_t reg_addr,
                                 const uint8_t *data, uint16_t len)
{
    g_mock_i2c.last_dev_addr = dev_addr;
    g_mock_i2c.last_pointer  = reg_addr;
    g_mock_i2c.write_call_count++;

    if (g_mock_i2c.force_comm_error)
    {
        return ADS1X1X_ERROR_COMM;
    }
    if (data == NULL || len != 2u || reg_addr > ADS1X1X_REG_HI_THRESH)
    {
        return ADS1X1X_ERROR_COMM;
    }
    /* Conversion register donanimda read-only'dir; mock de bunu yansitir. */
    if (reg_addr == ADS1X1X_REG_CONVERSION)
    {
        return ADS1X1X_ERROR_COMM;
    }

    uint16_t value = (uint16_t)((data[0] << 8) | data[1]);
    g_mock_i2c.registers[reg_addr] = value;
    return ADS1X1X_OK;
}

ads1x1x_status_t mock_i2c_read(uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len)
{
    g_mock_i2c.last_dev_addr = dev_addr;
    g_mock_i2c.last_pointer  = reg_addr;
    g_mock_i2c.read_call_count++;

    if (g_mock_i2c.force_comm_error)
    {
        return ADS1X1X_ERROR_COMM;
    }
    if (data == NULL || len != 2u || reg_addr > ADS1X1X_REG_HI_THRESH)
    {
        return ADS1X1X_ERROR_COMM;
    }

    uint16_t value = g_mock_i2c.registers[reg_addr];
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)(value & 0xFFu);
    return ADS1X1X_OK;
}
