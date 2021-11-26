
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <i2c_linux.h>

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
