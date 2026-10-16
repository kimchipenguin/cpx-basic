/* ext.h - Pure C's EXT.H for the GCC build; ltoa()/ultoa() are in tosglue.c */
#ifndef EXT_H
#define EXT_H
char *ltoa(long value, char *string, int radix);
char *ultoa(unsigned long value, char *string, int radix);
#endif
