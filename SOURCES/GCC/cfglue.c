/*
 * cfglue.c - the assembler routines of the 68000 builds in C, for
 * ColdFire (FireBee build)
 *
 * cpxmains.S and cpx_bios.S use 68000 instructions that ColdFire
 * doesn't have (movem -(sp), dbra/dbeq, exg, word arithmetic). For the
 * FireBee these are here instead, with the same behaviour:
 *  - BreakBASIC, ContBASIC, WaitBASIC, basic_exit around the context
 *    switch in cfcoro.S
 *  - the NULL-safe string functions with the 255 character limit that
 *    respect the NULL marker (BAS_strcpy etc., see GLO.H)
 *  - is0byte, make0byte, peekpoke
 */
#include <stddef.h>
#include <osbind.h>

#define NULL_KENN 0x87916543UL		/* marker behind a CHR$(0) */

long cf_to_basic(void);
long cf_to_gem(long value);

/*--------------------------------------------------------------------
 * Coroutine (was cpxmains.S)
 *------------------------------------------------------------------*/

static long systime;			/* _hz_200 when BASIC was continued */

static long hz200(void)
{
	return *(volatile long *)0x4ba;
}

/* BASIC waits: back to the GEM side */
void WaitBASIC(void)
{
	cf_to_gem(0);
}

/* switch to the GEM side if a key is waiting or 50 ms have passed */
void BreakBASIC(void)
{
	long now;

	if (Bconstat(2)) {
		WaitBASIC();
		return;
	}
	now = Supexec(hz200);
	if (now < systime || now >= systime + 10)
		WaitBASIC();
}

/* continue BASIC; 'STOP' when it has ended */
long ContBASIC(void)
{
	systime = Supexec(hz200);
	return cf_to_basic();
}

/* exit() of the BASIC (P2C.H): back for good, with 'STOP' */
void basic_exit(int code)
{
	(void)code;
	for (;;)
		cf_to_gem(0x53544F50L);		/* 'STOP' */
}

/*--------------------------------------------------------------------
 * String functions (were in cpx_bios.S)
 *------------------------------------------------------------------*/

static int marker(const char *p)
{
	const unsigned char *u = (const unsigned char *)p;

	return u[0] == (unsigned char)(NULL_KENN >> 24) &&
		u[1] == (unsigned char)(NULL_KENN >> 16) &&
		u[2] == (unsigned char)(NULL_KENN >> 8) &&
		u[3] == (unsigned char)NULL_KENN;
}

/* 4 if a NULL marker follows (the zero byte doesn't end the string) */
short is0byte(char *adr)
{
	return marker(adr) ? 4 : 0;
}

void make0byte(char *adr)
{
	adr[0] = (char)(NULL_KENN >> 24);
	adr[1] = (char)(NULL_KENN >> 16);
	adr[2] = (char)(NULL_KENN >> 8);
	adr[3] = (char)NULL_KENN;
	adr[4] = 0;
}

/* copies at most n characters; a zero followed by the marker is
   copied as a character */
static char *copy(char *d, const char *s, long n)
{
	for (;;) {
		char c = 0;
		while (n-- > 0) {
			c = *s++;
			*d++ = c;
			if (c == 0)
				break;
		}
		if (c == 0 && n > 0 && marker(s))
			continue;
		break;
	}
	*d = 0;
	return d;
}

char *BAS_strncpy(char *d, const char *s, size_t n)
{
	if (d != NULL && s != NULL)
		copy(d, s, (short)n);
	return d;
}

char *BAS_strcpy(char *d, const char *s)
{
	return BAS_strncpy(d, s, 254);
}

char *BAS_strncat(char *d, const char *s, size_t n)
{
	char *e = d;

	if (d == NULL || s == NULL)
		return d;
	for (;;) {
		while (*e++)
			;
		if (!marker(e))
			break;
	}
	copy(e - 1, s, (short)n);
	return d;
}

char *BAS_strcat(char *d, const char *s)
{
	return BAS_strncat(d, s, 254);
}

size_t BAS_strlen(const char *s)
{
	size_t n = 0;

	if (s == NULL)
		return 0;
	for (;;) {
		while (s[n])
			n++;
		if (!marker(s + n + 1))
			break;
		n++;
	}
	return n;
}

/*--------------------------------------------------------------------
 * PEEK/POKE: copies with the interrupts off (called via Supexec)
 *------------------------------------------------------------------*/

void peekpoke(char *s, char *d, int bytes)
{
	unsigned short sr;

	__asm__ volatile ("move.w %%sr,%0" : "=d" (sr));
	__asm__ volatile ("move.w #0x2700,%%sr" : : : "memory");
	while (bytes-- > 0)
		*d++ = *s++;
	__asm__ volatile ("move.w %0,%%sr" : : "d" (sr) : "memory");
}
