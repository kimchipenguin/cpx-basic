/*
 * tosglue.c - Pure C library functions that MiNTLib lacks
 *
 * BASIC.C declares the GEMDOS/BIOS/XBIOS calls it uses itself ("can't
 * include tos.h") and expects real functions, as in Pure C's
 * PCTOSLIB.LIB. MiNTLib only has inline macros, so here they are as
 * functions. Also ltoa() and ultoa() from Pure C's EXT.H.
 */
#include <osbind.h>

static long os_Supexec(long (*func)(void)) { return Supexec(func); }
static long os_Random(void) { return Random(); }
static long os_Bconin(int dev) { return Bconin(dev); }
static void os_Bconout(int dev, int c) { Bconout(dev, c); }
static int os_Bconstat(int dev) { return Bconstat(dev); }
static long os_Bcostat(int dev) { return Bcostat(dev); }
static int os_Dgetdrv(void) { return Dgetdrv(); }
static int os_Dgetpath(char *path, int drv) { return Dgetpath(path, drv); }
static long os_Dsetdrv(int drv) { return Dsetdrv(drv); }
static int os_Dsetpath(const char *path) { return Dsetpath(path); }
static int os_Fdelete(const char *name) { return Fdelete(name); }
static void os_Cconws(const char *s) { Cconws(s); }
static long os_Fattrib(const char *n, int w, int a) { return Fattrib(n, w, a); }

#undef Supexec
#undef Random
#undef Bconin
#undef Bconout
#undef Bconstat
#undef Bcostat
#undef Dgetdrv
#undef Dgetpath
#undef Dsetdrv
#undef Dsetpath
#undef Fdelete
#undef Cconws
#undef Fattrib

long Supexec(long (*func)(void)) { return os_Supexec(func); }
long Random(void) { return os_Random(); }
long Bconin(int dev) { return os_Bconin(dev); }
void Bconout(int dev, int c) { os_Bconout(dev, c); }
int Bconstat(int dev) { return os_Bconstat(dev); }
long Bcostat(int dev) { return os_Bcostat(dev); }
int Dgetdrv(void) { return os_Dgetdrv(); }
int Dgetpath(char *path, int drv) { return os_Dgetpath(path, drv); }
long Dsetdrv(int drv) { return os_Dsetdrv(drv); }
/* BASIC.C declares a second argument, which Pure C's binding ignores */
int Dsetpath(char *path, int dummy) { (void)dummy; return os_Dsetpath(path); }
int Fdelete(const char *name) { return os_Fdelete(name); }
void Cconws(char *s) { os_Cconws(s); }
long Fattrib(const char *name, int wflag, int attrib) { return os_Fattrib(name, wflag, attrib); }

char *ultoa(unsigned long value, char *string, int radix)
{
	char buf[33];
	char *p = buf + sizeof(buf) - 1;
	char *s = string;

	if (radix < 2 || radix > 36) {
		*string = 0;
		return string;
	}
	*p = 0;
	do {
		int d = (int)(value % radix);
		*--p = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		value /= radix;
	} while (value != 0);
	while ((*s++ = *p++) != 0)
		;
	return string;
}

char *ltoa(long value, char *string, int radix)
{
	if (value < 0 && radix == 10) {
		*string = '-';
		ultoa(-(unsigned long)value, string + 1, radix);
		return string;
	}
	return ultoa((unsigned long)value, string, radix);
}
