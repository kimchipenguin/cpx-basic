/*
 * portab.h - Pure C's PORTAB.H for the m68k-atari-mint GCC build.
 *
 * Pure C's int is 16 bits, so WORD is "int" there. GCC uses 32-bit
 * int (MiNTLib has no -mshort libc), so WORD has to be short here to
 * keep the AES/VDI and XControl structures the same size.
 */
#ifndef PORTAB_H
#define PORTAB_H

#define GLOBAL
#define LOCAL   static
#define EXTERN  extern
#define CDECL
#define VOID    void

typedef char           BYTE;
typedef unsigned char  UBYTE;
typedef short          WORD;
typedef unsigned short UWORD;
typedef long           LONG;
typedef unsigned long  ULONG;
typedef short          BOOLEAN;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#endif
