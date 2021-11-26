#include <inttypes.h>
#include <avr/io.h>

#include <i2c.h>

#include <bits.h>

#define DLEVEL 0
#include <serdeb.h>

uint8_t i2flags=0x80; /* Zero initialised fails - compiler bug? */
#define F_START 1 /* Used to detect & handle repeated start condition. */

static int8_t i2c_chkstatus(const uint8_t i2c_status)
{
	uint8_t v = TWSR & 0xF8;
	if (v != i2c_status) {
		DBG1("TWI: ERR TWSR=%02x\n", TWSR & 0xF8);
		return v;
	}
	return 0;
}

static void i2c_wait(void)
{
	/* Wait for done */
	while (!(TWCR & (1<<TWINT)));
}

static uint8_t i2c_start(void)
{
	uint8_t rval;
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	i2c_wait();
	DBG2("%d flags=%02x\n", __LINE__, i2flags);
	if (i2flags & F_START) {
		rval=i2c_chkstatus(I2CS_RSTART);
		DBG2("RSTART(%d)\n", rval);
		if (rval) return rval;
	} else {
		rval=i2c_chkstatus(I2CS_START);
		DBG2("START(%d)\n", rval);
		if (rval) return rval;
		i2flags |= F_START;
		DBG2("%d flags=%02x\n", __LINE__, i2flags);
	}
	return 0;
}

static uint8_t i2c_addr(const uint8_t a_rw)
{
	TWDR = a_rw;
	TWCR = (1<<TWINT) | (1<<TWEN);
	i2c_wait();
	if (a_rw & I2C_RD)
		return i2c_chkstatus(I2CS_MRSLA);
	else
		return i2c_chkstatus(I2CS_MTSLA);
}

uint8_t i2c_dtaw(const uint8_t d)
{
	TWDR = d;
	TWCR = (1<<TWINT) | (1<<TWEN);
	i2c_wait();
	return i2c_chkstatus(I2CS_MTDTA);
}

/* togo = 1 last byte, send NACK */
uint8_t i2c_dtar(uint8_t *d, uint8_t togo)
{
	if (togo==1)
		TWCR = (1<<TWINT) | (1<<TWEN);
	else
		TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	i2c_wait();
	*d=TWDR;
	if (togo==1)
		return(i2c_chkstatus(I2CS_MRDTAN));
	else
		return(i2c_chkstatus(I2CS_MRDTA));
}

void i2cm_stop(void)
{
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
	i2flags &= ~F_START;
}

/* Master Write specified number of bytes to slave. No stop is sent. */
/* Returns 0 on success or failed status code */
uint8_t i2cm_write(const uint8_t addr, const uint8_t *data, uint8_t bytes)
{
	uint8_t rval;
	DBG1("I2C(W): addr=%02x, bytes=%d\n", addr, bytes);
	DBG2("%d flags=%02x\n", __LINE__, i2flags);
	rval=i2c_start();
	DBG2("%d flags=%02x\n", __LINE__, i2flags);
	if (rval) return rval;
	rval=i2c_addr(addr);
	if (rval) return rval;
	while (bytes--) {
		rval=i2c_dtaw(*data++);
		if (rval) return rval;
	}
	return 0;
}

/* Master Read specified number of bytes from slave. No stop is sent. */
/* Returns 0 on success or failed status code */
uint8_t i2cm_read(const uint8_t addr, uint8_t *data, uint8_t bytes)
{
	uint8_t rval;
	DBG1("I2C(R): addr=%02x, bytes=%d\n", addr, bytes);
	DBG2("%d flags=%02x\n", __LINE__, i2flags);
	rval=i2c_start();
	DBG2("%d flags=%02x\n", __LINE__, i2flags);
	if (rval) return rval;
	rval=i2c_addr(addr | I2C_RD);
	if (rval) return rval;
	while(bytes) {
		rval=i2c_dtar(data, bytes);
		data++;
		if (rval) return rval;
		bytes--;
	}
	return 0;
}
