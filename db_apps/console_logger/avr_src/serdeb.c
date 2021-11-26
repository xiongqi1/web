/* The following defines (set in makefile) change stuff.

SD_NOFMT	= No formatting whatsoever, simple strings, followed by individual on switches.
  SD_FMTINT	= Allow integer format %d, %u
  SD_FMTCHR	= Allow character number modifier
  SD_FMTHEX	= Allow hex and pointer formatting %x, %p
  SD_FMTFLT	= Allow float formatting %f
  SD_FMTBIN	= Allow binary formatting %b (default off)

*/

#ifndef SD_NOFMT
	#define SD_FMTINT
	#define SD_FMTCHR
	#define SD_FMTHEX
	#define SD_FMTFLT
	/* #define SD_FMTBIN */
#endif

#include <stdint.h>
#include <stdarg.h>
#include <math.h>

#include <serdeb.h>

#ifndef TEST

	#include <avr/pgmspace.h>
	#include <avr/io.h>

	#if !defined(DADDR) || !defined(DPIN)
		#error 'Please define DADDR & DPIN'
		/* Example */
		#define DADDR 0x1B
		#define DPIN  PA7
	#endif

	#include <serdeb.h>

	/* Automatically generated serial output routine */
	#include <sout.h>

#else

	#include <stdio.h>

	void debugchar(uint8_t v)
	{
		putchar(v);
	}

	uint8_t pgm_read_byte_near(const char *s)
	{
		return (uint8_t)*s;
	}

	#define PSTR(s) (s)

#endif /* TEST only */



static void print(const char *s)
{
	while (*s) {
		debugchar((uint8_t)*s);
		s++;
	}
}

void print_prog(const char *s)
{
	uint8_t c;
	c = pgm_read_byte_near(s);
	while (c) {
		debugchar(c);
		s++;
		c = pgm_read_byte_near(s);
	}
}

#ifdef SD_FMTBIN
static void printbin8(uint8_t v)
{
	uint8_t z = 8;
	while (z--) {
		debugchar((v & 0x80)?'1':'0');
		v<<=1;
	}
}
#endif /* SD_FMTBIN */

#ifdef SD_FMTHEX
static inline char hexchar(const uint8_t v)
{
	return ((v>9)?'a'+v-10:'0'+v);
}

static void printhex8(uint8_t x)
{
	debugchar(hexchar(x >> 4));
	debugchar(hexchar(x & 0xf));
}

static void printhex16(uint16_t x)
{
	printhex8(x >> 8);
	printhex8(x & 0xff);
}

static void printhex32(uint32_t x)
{
	printhex16(x >> 16);
	printhex16(x & 0xffff);
}
#endif

#ifdef SD_FMTINT
static void printint(int16_t i)
{
	uint16_t q;
	int16_t d=10000;
	if (i<0) { debugchar('-'); i=-i; }
	/* skip leading zeros */
	q=i/d;
	while ((q==0) && (d>=10)) {
		d=d/10;
		q=i/d;
	};
	while (d>0) {
		debugchar('0'+(char)q);
		i -= q*d;
		d=d/10;
		if (d) q=i/d;
	}
}
#endif

#ifdef SD_FMTFLT
static void printfloat(double v, uint8_t frac)
{
	int16_t i;
	if (v<0) {
		debugchar('-');
		v=-v;
	}
	i = trunc(v);
	printint(i);
	v -= i;
	debugchar('.');
	while (frac--) {
		v*=10.0;
		i = trunc(v);
		v -= i;
		debugchar('0' + i);
	}
}
#endif

/* Compile with -no-builtin-printf to avoid warning. fmt string must be in progmem. */
/* Formatted hex strings must be %2x, %4x or %8x. Default is %4x */
/* float/double are printed with specified number of fractional digits, default 2 */
void s_printf(const char *fmt, ...)
{
	uint8_t chars=0;
	char c;
	va_list ap;
	va_start(ap, fmt);

	c = pgm_read_byte_near(fmt);
	while (c) {
		if (c == '%') {
			chars = 0;
			while(1) {
				fmt++; c = pgm_read_byte_near(fmt);
				switch (c) {
#ifdef SD_FMTINT
					case 'd' :
					case 'u' :
						printint((int16_t)va_arg(ap, int));
						break;
#endif
#ifdef SD_FMTBIN
					case 'b' :
						printbin8((uint8_t)va_arg(ap, unsigned int));
						break;
#endif
#ifdef SD_FMTHEX
					case 'x' :
						switch (chars) {
							case 2:
								printhex8((uint8_t)va_arg(ap, int));
								break;
							case 8:
								printhex32(va_arg(ap, uint32_t));
								break;
							default:
								printhex16((uint16_t)va_arg(ap, unsigned int));
								break;
						}
						break;
					case 'p' :
						if (sizeof(void*)==2)
							printhex16((uint16_t)va_arg(ap, unsigned int));
						else
							printhex32(va_arg(ap, uint32_t));
						break;
#endif
					case 's' :
						print(va_arg(ap, const char *));
						break;
					case 'c' :
						debugchar((uint8_t)va_arg(ap, int));
						break;
#ifdef SD_FMTHEX
					case '.' :
						chars = 0;
						continue;
						break;
#endif
#ifdef SD_FMTFLT
					case 'f' :
						if (chars==0) chars=2;
						printfloat(va_arg(ap, double), chars);
						break;
#endif
					default :
#ifdef SD_FMTCHR
						if (c>='0' || c<='9') {
							chars = chars*10+(c-'0');
							continue;
						} else
#endif
						{
							print_prog(PSTR("??"));
						}
						break;
				}
				goto get_out;
			};
get_out:;
		} else {
			debugchar(c);
		}
		fmt++; c = pgm_read_byte_near(fmt);
	}
	va_end(ap);
}

#ifdef TEST

int main(void)
{
	DBG("Hello World\n");
	DBG("String: \"%s\"\n", "somestring");
	DBG("Char: '%c'\n", '@');
	DBG("Ints: %d %d %d %d\n", 47, -11, 4711, -4711);
	DBG("Floats: %f %f %f %f\n", 12.34, 0.002, -0.01, -1e-1);
	DBG("Hex: %x\n", 0x4711);
	DBG("Fmtchars: '%8x' '%5f'\n", 0xdead, 123.45678);
	return 0;
}

#endif
