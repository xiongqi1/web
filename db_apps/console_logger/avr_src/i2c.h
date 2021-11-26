#ifndef _I2C_H
#define _I2C_H

#include <inttypes.h>

/* Status decode for master */

#define I2CS_START  0x08 /* Start condition transmitted */
#define I2CS_RSTART 0x10 /* Repeated start condition done */
#define I2CS_MTSLA  0x18 /* Master TX Address sent, ACK */
#define I2CS_MTSLAN 0x20 /* Master TX Address sent, NACK */
#define I2CS_MTDTA  0x28 /* Master TX Data byte transmitted, ACK */
#define I2CS_MTDTAN 0x30 /* Master TX Data byte transmitted. NACK */
#define I2CS_MTARB  0x38 /* Arbitration lost */

#define I2CS_MRSLA  0x40 /* Master RX Address sent. ACK */
#define I2CS_MRSLAN 0x48 /* Master RX Address sent. NACK */
#define I2CS_MRDTA  0x50 /* Master RX Data byte received. ACK sent. */
#define I2CS_MRDTAN 0x58 /* Master RX Data byte received. NACK sent. */

/* Status decode for slave (TODO) */


/* OR to slave address */
#define I2C_RD 1
#define I2C_WR 0

/* TODO: use interrupts & callback. */

/* WARNING: Operations get stuck if there are no PU resistors */

/* Set up I2C master clock generator to specified frequency. */
static void inline i2c_clock(const uint32_t freq)
{
	TWBR = (uint8_t)((F_CPU/freq-16)/2);
	TWSR = 0;
}

/* Master Write specified number of bytes to slave. No stop is sent. */
/* Returns 0 on success or failed status code */
uint8_t i2cm_write(const uint8_t addr, const uint8_t *data, uint8_t bytes);
/* Send single additional byte */
uint8_t i2c_dtaw(const uint8_t d);

/* Master Read specified number of bytes from slave. No stop is sent. */
/* Returns 0 on success or failed status code */
uint8_t i2cm_read(const uint8_t addr, uint8_t *data, uint8_t bytes);
/* Read additional byte. If togo==1, send NACK (last byte) */
uint8_t i2c_dtar(uint8_t *d, uint8_t togo);

/* Stop condition, call after end of transaction */
void i2cm_stop(void);

#endif /* _I2C_H */
