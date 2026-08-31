/**
 * @file mock_i2c.h
 * @brief Testlerde gercek I2C donanimi yerine kullanilan sahte (mock) I2C
 *        veriyolu. ads1x1x driver'inin read/write callback imzalarina
 *        (ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn) birebir uyar.
 *
 * Register haritasini (Conversion/Config/Lo_thresh/Hi_thresh, 16-bit,
 * pointer register uzerinden secilen) basitce RAM'de taklit eder --
 * boylece driver'in gercekten dogru byte dizisini uretip uretmedigini
 * ve okurken dogru sekilde yorumlayip yorumlamadigini test edebiliriz.
 */
#ifndef MOCK_I2C_H_
#define MOCK_I2C_H_

#include "ads1x1x.h"

/* Testin, mock'un davranisini kontrol edebilmesi icin durum */
typedef struct {
    uint16_t registers[4];       /* Conversion, Config, Lo_thresh, Hi_thresh */
    uint8_t  last_pointer;       /* son yazilan/kullanilan register adresi */
    uint8_t  last_dev_addr;      /* driver'in gonderdigi I2C adresi (test dogrulamasi icin) */
    int      write_call_count;
    int      read_call_count;
    bool     force_comm_error;   /* true ise sonraki cagri ADS1X1X_ERROR_COMM doner */
} mock_i2c_state_t;

extern mock_i2c_state_t g_mock_i2c;

/** Mock'u sifirlar, register'lari datasheet reset degerlerine dondurur. */
void mock_i2c_reset(void);

/** ads1x1x_i2c_write_fn imzasina uyan mock yazma fonksiyonu. */
ads1x1x_status_t mock_i2c_write(uint8_t dev_addr, uint8_t reg_addr,
                                 const uint8_t *data, uint16_t len);

/** ads1x1x_i2c_read_fn imzasina uyan mock okuma fonksiyonu. */
ads1x1x_status_t mock_i2c_read(uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len);

#endif /* MOCK_I2C_H_ */
