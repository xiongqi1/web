#ifndef FIFO_H
#define FIFO_H

#include <inttypes.h>

#ifndef FIFO_SIZE
#define FIFO_SIZE 128
#endif

#if FIFO_SIZE < 255
typedef uint8_t itype;
#else
typedef uint16_t itype;
#endif

/* Fifo is empty if r=w. */

struct fifo {
	itype r;		/* read index */
	itype w;		/* write index */
	uint8_t d[FIFO_SIZE+1];	/* One byte spare */
};

static inline void ff_init(struct fifo *f)
{
	f->r = f->w = 0;
}

static inline void ff_put(struct fifo *f, uint8_t v)
{
	f->d[f->w++] = v;
	if (f->w >= FIFO_SIZE+1) f->w=0;
}

static inline uint8_t ff_get(struct fifo *f)
{
	uint8_t r;
	r = f->d[f->r++];
	if (f->r >= FIFO_SIZE+1) f->r=0;
	return r;
}

/* True if FIFO is empty */
static inline uint8_t ff_empty(struct fifo *f)
{
	return (f->r == f->w);
}

/* Returns number of bytes in FIFO */
static inline itype ff_level(struct fifo *f)
{
	itype r;
	if (f->r <= f->w) {
		r = f->w - f->r;
	} else {
		r = FIFO_SIZE - f->r + f->w + 1;
	}
	return r;
}

static inline uint8_t ff_full(struct fifo *f)
{
	return (ff_level(f) == FIFO_SIZE);
}

static inline void ff_dump(struct fifo *f, int (*pr)(const char *, ...))
{
	uint16_t i;
	pr("f=%d, w=%d, r=%d\n", ff_level(f), f->w, f->r);
	for (i=0; i<FIFO_SIZE+1; i++) {
		pr(" %02x", f->d[i]);
	}
	pr("\n");
	for (i=0; i<FIFO_SIZE+1; i++) {
		if (i==f->w && i==f->r)
			pr(" wr");
		else if (i==f->r)
			pr("  r");
		else if (i==f->w)
			pr(" w ");
		else
			pr("   ");
	}
	pr("\n");
}

#endif /* FIFO_H */
