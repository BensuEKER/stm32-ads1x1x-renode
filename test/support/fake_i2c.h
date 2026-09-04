/**
 * @file fake_i2c.h
 * @brief Hand-written fake I2C bus used by the Ceedling test suite for the
 *        ads1x1x driver (no real hardware or Renode dependency).
 *
 * Named "fake_i2c" (not "mock_i2c") on purpose: this is a hand-written
 * stand-in, not a CMock-auto-generated mock (CMock treats any included
 * header named "mock_X.h" as an auto-generated mock of "X.h" and tries
 * to locate that real header, which breaks for a hand-written fake).
 *
 * This matches the driver's I2C callback signatures
 * (ads1x1x_i2c_read_fn / ads1x1x_i2c_write_fn) so the driver can be
 * exercised exactly as it would be on real hardware, just against an
 * in-memory register model instead of a physical I2C bus.
 */
#ifndef FAKE_I2C_H_
#define FAKE_I2C_H_

#include "ads1x1x.h"

typedef struct {
    uint16_t registers[4]; /* Conversion, Config, Lo_thresh, Hi_thresh */
    uint8_t  last_pointer;
    uint8_t  last_dev_addr;
    int      write_call_count;
    int      read_call_count;
    bool     force_comm_error;
} fake_i2c_state_t;

extern fake_i2c_state_t g_fake_i2c;

void fake_i2c_reset(void);

ads1x1x_status_t fake_i2c_write(uint8_t dev_addr, uint8_t reg_addr,
                                 const uint8_t *data, uint16_t len);

ads1x1x_status_t fake_i2c_read(uint8_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len);

#endif /* FAKE_I2C_H_ */
