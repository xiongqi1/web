#ifndef __AT91REG_H__
#define __AT91REG_H__


#define __phys_readl(base,offset)				*(volatile long*)((size_t)(base)+(offset))
#define __phys_writel(base,offset,value)	*(volatile long*)((size_t)(base)+(offset))=value;

#define PIOA 0x400
#define PIOB 0x600
#define PIOC 0x800

/* GPIO enable / !Mux */
#define PER  0x00
#define PDR  0x04
#define PSR  0x08
/* out / !in */
#define OER  0x10
#define ODR  0x14
#define OSR  0x18
/* Glitch filter */
#define IFER 0x20
#define IFDR 0x24
#define IFSR 0x28
/* Data register */
#define SODR 0x30
#define CODR 0x34
#define ODSR 0x38
#define PDSR 0x3C /* Pin readback (in) */
/* Interrupt enable */
#define IER  0x40
#define IDR  0x44
#define IMR  0x48
#define ISR  0x4C
/* MultiDrive (OC) */
#define MDER 0x50
#define MDDR 0x54
#define MDSR 0x58
/* Pull up (inverted) */
#define PUDR 0x60
#define PUER 0x64
#define PUSR 0x68
/* Mux peripheral select A/!B */
#define ASR  0x70
#define BSR  0x74
#define ABSR 0x78
/* Bitmask for direct writes to ODSR */
#define OWER 0xA0
#define OWDR 0xA4
#define OWSR 0xA8

#endif
