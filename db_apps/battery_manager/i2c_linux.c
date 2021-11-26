
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "i2c_linux.h"

int i2c_open(struct i2c_handle *H, uint8_t busno, uint8_t devaddr)
{
	char fname[16];
	snprintf(fname, sizeof(fname), "/dev/i2c-%d", busno);
	H->fd = open(fname, O_RDWR);
	if (H->fd < 0) return H->fd;
	H->addr = devaddr;
	H->ioctl.msgs = H->msgs;
	return 0;
}

void i2c_close(struct i2c_handle *H)
{
	close(H->fd);
	H->fd = -1; // Poison the file descriptor
}

/* I2C start - write - regaddr - writedata */
int i2c_writeblock(struct i2c_handle *H, uint8_t reg, const uint8_t *data, int bytes)
{
	auto uint8_t buf[bytes+1];
	int r;

	buf[0] = reg;
	memcpy(&buf[1], data, bytes);

	// Setting up write transfer, single transaction
	H->ioctl.msgs[0].addr = H->addr;
	H->ioctl.msgs[0].flags = 0;
	H->ioctl.msgs[0].len = bytes+1;
	H->ioctl.msgs[0].buf = buf;
	H->ioctl.nmsgs = 1;

	r = ioctl(H->fd, I2C_RDWR, &H->ioctl);

	return (r < 0)?r:0;
}

/* I2C start - write - regaddr - restart - read - readdata */
int i2c_readblock(struct i2c_handle *H, uint8_t reg, uint8_t *data, int bytes)
{
	uint8_t ra = reg;
	int r;

	// Setting up read transfer, write(reg) + read(data)
	// Using a repeated start between the blocks
	H->ioctl.msgs[0].addr = H->addr;
	H->ioctl.msgs[0].flags = 0;
	H->ioctl.msgs[0].len = 1;
	H->ioctl.msgs[0].buf = &ra;
	H->ioctl.msgs[1].addr = H->addr;
	H->ioctl.msgs[1].flags = I2C_M_RD;
	H->ioctl.msgs[1].len = bytes;
	H->ioctl.msgs[1].buf = data;
	H->ioctl.nmsgs = 2;

	r = ioctl(H->fd, I2C_RDWR, &H->ioctl);

	return (r < 0)?r:0;
}

/* Operation wrappers for specific types, including endianness conversion */
int i2c_write_8(struct i2c_handle *H, uint8_t reg, uint8_t data)
{
	return i2c_writeblock(H, reg, &data, 1);
}

int i2c_write_be16(struct i2c_handle *H, uint8_t reg, uint16_t data_be)
{
	uint8_t d[2];
	d[0] = data_be >> 8;
	d[1] = data_be & 0xff;
	return i2c_writeblock(H, reg, d, sizeof(d));
}

int i2c_write_le16(struct i2c_handle *H, uint8_t reg, uint16_t data_le)
{
	uint8_t d[2];
	d[0] = data_le & 0xff;
	d[1] = data_le >> 8;
	return i2c_writeblock(H, reg, d, sizeof(d));
}

/* Returns >= 0 value or < 0 for error */
int i2c_read_8(struct i2c_handle *H, uint8_t reg)
{
	uint8_t v;
	int r;
	r = i2c_readblock(H, reg, &v, 1);
	if (r) return -1;
	return (int)v;
}

/* Returns >= 0 value or < 0 for error */
int i2c_read_le16(struct i2c_handle *H, uint8_t reg)
{
	int r;
	uint16_t v = 0;
	uint8_t d[2];
	r = i2c_readblock(H, reg, d, 2);
	if (r) return r;
	v <<= 8; v |= d[1];
	v <<= 8; v |= d[0];
	return (int)v;
}

int i2c_read_le32(struct i2c_handle *H, uint8_t reg, uint32_t *vp)
{
	int r;
	uint32_t v = 0;
	uint8_t d[4];
	r = i2c_readblock(H, reg, d, 4);
	if (r) return r;
	v <<= 8; v |= d[3];
	v <<= 8; v |= d[2];
	v <<= 8; v |= d[1];
	v <<= 8; v |= d[0];
	*vp = v;
	return r;
}

int i2c_update_bits(struct i2c_handle *H, uint8_t reg, uint8_t mask, uint8_t data)
{
	int ret;
	ret = i2c_read_8(H, reg);
	if (ret < 0) {
		return -1;
	}
	ret &= ~mask;
	ret |= data & mask;

	return i2c_write_8(H, reg, ret);
}
