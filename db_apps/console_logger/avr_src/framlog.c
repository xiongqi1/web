#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/cpufunc.h>
#include <avr/wdt.h>

#include <bits.h>
#include <serdeb.h>

#define FIFO_SIZE 100
#include <fifo.h>

#include <i2c.h>

#define MEMORY_BARRIER __asm__ __volatile__("":::"memory");

FUSES = {
	.low = /* CKSEL=1111, Low power crystal osc, 8MHz */
		FUSE_SUT1, /* Startup with BOD */
	
	.high = FUSE_BODLEVEL1 /* Brownout at 2.7V */
		& FUSE_SPIEN,  /* Enable serial programming. */

	.extended = EFUSE_DEFAULT, /* 1024 byte boot block, irrelevant. */
};

/* serial port buffers */

struct fifo TxFF;
struct fifo RxFF;

static void txbyte(uint8_t v)
{
	uint8_t full;
	/* Wait for space */
	do {
		ATOMIC(
			full = ff_full(&TxFF);
		);
	} while (full);

	ATOMIC(
		ff_put(&TxFF, v);
		/* Enable TX data register int */
		SET_BIT(UCSR0B, UDRIE0);
	);
}

static void txblock(const uint8_t *v, uint8_t bytes)
{
	while (bytes--) txbyte(*v++);
}

/* Tx Data register empty */
ISR(USART_UDRE_vect)
{
	uint8_t v;

	v = ff_get(&TxFF);
	if (ff_empty(&TxFF)) {
		/* This was the last byte, disable DRE interrupt. */
		CLR_BIT(UCSR0B, UDRIE0);
	}

	//DBG("TX %02x\n", v);

	/* Send byte */
	UDR0 = v;
}

/* UART receive ISR. */
ISR(USART_RX_vect)
{
	uint8_t c;

	/* Read data register */
	c=UDR0;

	/* TODO: Check for full. */
	ff_put(&RxFF, c);

	//DBG("RX %02x\n", c);
}


/* FRAM access */
#define FRAM_ADDR 0xA0
#define FRAMSIZE ((uint16_t)32*1024)

static uint8_t f_setaddr(uint16_t a)
{
	uint8_t v[2] = { a >> 8, a & 0xFF };
	return i2cm_write(FRAM_ADDR, v, 2);
}

static uint8_t f_blockwrite(uint16_t a, const uint8_t v[], uint8_t bytes)
{
	uint8_t rval = 0;
	rval = f_setaddr(a);
	while (bytes--)
		i2c_dtaw(*v++);
	i2cm_stop();
	return rval;
}

static uint8_t f_blockread(uint16_t a, uint8_t v[], uint8_t bytes)
{
	uint8_t rval = 0;
	rval = f_setaddr(a);
	/* Repeated start cond. */
	if (!rval)
		rval = i2cm_read(FRAM_ADDR, v, bytes);
	i2cm_stop();
	return rval;
}

static uint8_t f_clear(void)
{
	uint8_t rval = 0;
	uint16_t bytes = FRAMSIZE;
	rval = f_setaddr(0);
	if (!rval) {
		while (bytes--) i2c_dtaw(0);
	}
	i2cm_stop();
	return rval;
}

/* FRAM memory layout */
#define DATA_START 0x10      /* Allowing 16 bytes for header */
#define DATA_END   (uint16_t)FRAMSIZE  /* one beyond last index */
struct f_header {
	uint16_t dptr; /* Points behind newest data, updated every block. */
} hdr;

static uint8_t f_buffer[FIFO_SIZE]; /* Buffer for FRAM writing */
static uint8_t f_disabled = 0;      /* No recording */

static void f_write(uint8_t bytes)
{
	if (f_disabled) return;
	if (hdr.dptr + bytes >= DATA_END) {
		/* This write would wrap */
		uint16_t rem;
		rem = FRAMSIZE - hdr.dptr;
		f_blockwrite(hdr.dptr, f_buffer, rem);
		bytes -= rem;
		hdr.dptr = DATA_START;
		f_blockwrite(hdr.dptr, f_buffer+rem, bytes);
		hdr.dptr+=bytes;
	} else {
		/* No wrap */
		f_blockwrite(hdr.dptr, f_buffer, bytes);
		hdr.dptr+=bytes;
	}
	/* Update header */
	f_blockwrite(0, (uint8_t*)&hdr, sizeof(hdr));
}

static void f_dump(void)
{
	uint16_t p = hdr.dptr;
	uint16_t bytes;
	/*
	 * 1: Bytes to end of FRAM
	 */
	bytes = DATA_END - p;
	while (bytes) {
		uint16_t n = bytes > FIFO_SIZE ? FIFO_SIZE : bytes;
		f_blockread(p, f_buffer, n);
		txblock(f_buffer, n);
		bytes -= n;
		p += n;
	}

	/* Bytes after wrap */
	p = DATA_START;
	bytes = hdr.dptr - DATA_START - 1;
	while (bytes) {
		uint16_t n = bytes > FIFO_SIZE ? FIFO_SIZE : bytes;
		f_blockread(p, f_buffer, n);
		txblock(f_buffer, n);
		bytes -= n;
		p += n;
	}
}

#define MAGIC "Logger Dump"
#define LOGOFF "U-Boot"
static uint8_t magicstate=0; /* State machine tracking MAGIC */
static uint8_t logfstate=0;  /* State machine tracking LOGOFF */
static uint8_t magic(uint8_t c)
{
	/* Magic string is framed by newline characters */
	if (c=='\n' || c=='\r') {
		//DBG("NL %d\n", magicstate);
		if (magicstate==sizeof(MAGIC)-1) {
			/* Magic success */
			magicstate = 0;
			return 1;
		}
		magicstate = 0;
		return 0;
	}
	if (MAGIC[magicstate]==c) magicstate++;
	else magicstate=0;

	if (LOGOFF[logfstate]==c) {
		logfstate++;
		if (logfstate == sizeof(LOGOFF)-1) {
			logfstate = 0;
			return 2;
		}
	} else {
		logfstate=0;
	}

	return 0;
}

int main(void)
{
	/* Initialise globals. */
	ff_init(&TxFF);
	ff_init(&RxFF);

	PRR = (uint8_t)~(_BV(PRTWI) | _BV(PRUSART0)); /* Some clocks on. */

	/* Debug output */
	SET_BIT(DDRB,3);
	SET_BIT(PORTB,3);
	DBG("\nHello World\n");

	/* LED */
	SET_BIT(DDRD,2);
	SET_BIT(PORTD,2);

	/* Enable pull-up on Rx line. */
	CLR_BIT(DDRD, PD0);
	SET_BIT(PORTD, PD0);
	/* UART setup for 16MHz, 115200, 2x speed */
	UBRR0H = 0;
	UBRR0L = 16;
	UCSR0A = (1 << U2X0) | (1 << TXC0);
	/* Set frame format: 8data, 1 stop. */
	UCSR0C = (3 << UCSZ00) | (1 << USBS0);
	/* Rx interrupt on, transmitter & reciver on. */
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);

	/* I2C */
	SET_BIT(PORTC, 4); // Pullup
	SET_BIT(PORTC, 5); // Pullup
	i2c_clock(400000);

	/* Get header from previous run */
	f_blockread(0, (uint8_t*)&hdr, sizeof(hdr));
	if (hdr.dptr < DATA_START || hdr.dptr >= DATA_END) {
		hdr.dptr = DATA_START;
		/* Clear out memory */
		f_clear();
	}
	f_disabled = 0;

	/* Interrupts are go */
	sei();

	while(1) {
		uint8_t nrxb, i;
		ATOMIC(
			nrxb = ff_level(&RxFF);
		);
		for (i = 0; i < nrxb; i++) {
			uint8_t c;
			ATOMIC(
				c = ff_get(&RxFF);
			);
			/* TODO: Skip double newlines */
			DBG("%c", c);
			switch (magic(c)) {
				case 1:
					DBG("MAGIC, dumping\n");
					f_disabled = 0;
					f_dump();
					break;
				case 2:
					DBG("LOGOFF, stop record\n");
					f_disabled = 1;
					break;
			}
			f_buffer[i] = c;
		}
		f_write(nrxb);

		PORTD ^= _BV(2);
	}

	return (0);
}
