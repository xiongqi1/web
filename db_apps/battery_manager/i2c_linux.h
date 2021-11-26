/*
 * Small library for Linux userspace I2C drivers
 *
 * Iwo.Mergler@netcommwireless.com
 */
#ifndef _I2C_LINUX_H
#define _I2C_LINUX_H

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>

struct i2c_handle {
	int fd;
	uint8_t addr;
	struct i2c_msg msgs[2]; // We use at most two transfers (for read)
	struct i2c_rdwr_ioctl_data ioctl; // I2C IOCTL message array
};

/* If open returns < 0, use perror for info */
int i2c_open(struct i2c_handle *H, uint8_t busno, uint8_t devaddr);
void i2c_close(struct i2c_handle *H);

/* Block write, reg address + data in single transaction */
int i2c_writeblock(struct i2c_handle *H, uint8_t reg, const uint8_t *data, int bytes);

/* Block read, reg address write, repeated start, data block read */
int i2c_readblock(struct i2c_handle *H, uint8_t reg, uint8_t *data, int bytes);

/* write a single 8-bit value */
int i2c_write_8(struct i2c_handle *H, uint8_t reg, uint8_t data);

/* write 16-bit big endian */
int i2c_write_be16(struct i2c_handle *H, uint8_t reg, uint16_t data_be);

/* write 16-bit little endian */
int i2c_write_le16(struct i2c_handle *H, uint8_t reg, uint16_t data_le);

int i2c_read_8(struct i2c_handle *H, uint8_t reg);

/* read 16-bit little endian */
/* Returns >= 0 value or < 0 for error */
int i2c_read_le16(struct i2c_handle *H, uint8_t reg);

/* read 32-bit little endian */
int i2c_read_le32(struct i2c_handle *H, uint8_t reg, uint32_t *vp);

/* update certain bits in a register */
int i2c_update_bits(struct i2c_handle *H, uint8_t reg, uint8_t mask, uint8_t data);
#endif // !_I2C_LINUX_H
