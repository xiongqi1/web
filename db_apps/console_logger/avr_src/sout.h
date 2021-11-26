/* This code was generated automatically by debugout.tcl 16000000 921600 */
/* Sending at 921600 BAUD. Pin should be configured as output and driven 'high'. */
/* Please define DADDR & DPIN before including this code. e.g. DADDR=0x1B, DPIN=PA7 */


/* Stringify macros */
#define s(x) #x
#define S(X) s(X)

/* send a single character */ 
void debugchar(uint8_t v)
{

	/* Registers:
		22 = cycle delay counter
		23 = port value shadow
		24 = argument, v
		25 = Flag save
	*/
	asm volatile (
	"	mov r24,%0\n"
	/* Save flags, disable ints. */
	"	in	r25, 0x3f\n"
	"	cli\n"

	/* Start bit */
	"	cbi	" S(DADDR) "," S(DPIN) "\n"	/* 2 cy */

	/* Prepare port shadow register */
	"	in	r23," S(DADDR) "\n"		/* 1 cy */

	/* Start bit delay, 17.36 + 0.00 = 15 + 0.36 */
	"	ldi r22, 5\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* D0 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D0 delay, 17.36 + 0.36 = 13 + -0.64 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */
	"	nop\n"		/* 1 cy */

	/* D1 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D1 delay, 17.36 + -0.64 = 12 + 0.36 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* D2 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D2 delay, 17.36 + 0.36 = 13 + -0.64 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */
	"	nop\n"		/* 1 cy */

	/* D3 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D3 delay, 17.36 + -0.64 = 12 + 0.36 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* D4 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D4 delay, 17.36 + 0.36 = 13 + -0.64 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */
	"	nop\n"		/* 1 cy */

	/* D5 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D5 delay, 17.36 + -0.64 = 12 + 0.36 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* D6 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D6 delay, 17.36 + 0.36 = 13 + -0.64 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */
	"	nop\n"		/* 1 cy */

	/* D7 */
	"	cbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	sbrc	r24, 0\n"			/* 1/2 cy */
	"	sbr	r23," S(_BV(DPIN)) "\n"		/* 1 cy */
	"	out	" S(DADDR) ",r23\n"		/* 1 cy */
	"	lsr	r24\n"				/* 1 cy => 5 cy total */

	/* D7 delay, 17.36 + -0.64 = 12 + 0.36 */
	"	ldi r22, 4\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* Stop bit */
	"	sbi	" S(DADDR) "," S(DPIN) "\n"	/* 2 cy */

	/* Stop bit (incuding re-call for 2 stop bits) delay, 17.36 + 0.00 = 3 + 0.36 */
	"	ldi r22, 1\n"	/* 1cy */
	"1:	dec r22\n"	/* 1cy */
	"	brne 1b\n"	/* 2cy (1 on last) */

	/* Restore flags */
	"	out 0x3f, r25\n"
	:
	: "r" (v)
	: "r22", "r23", "r24", "r25"
	);

}
