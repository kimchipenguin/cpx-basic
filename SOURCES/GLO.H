#ifndef ___GLO_H___
#define ___GLO_H___

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <aes.h>
#include <vdi.h>
#include <portab.h>

#define NUM_EL(a) (sizeof(a)/sizeof(*a))

#define REDEFD

#ifdef  REDEFD
#	ifdef stdin
#		undef stdin
#	endif

#	ifdef stdout
#		undef stdout
#	endif

#	define stdin NULL
#	define stdout NULL
#endif

/********************************************************************/
#define TIMEOUT_BASIC	50
#define MAX_CX			32
#define MAX_CY			22

#define KB_SIZE			128

/* cursor up/down in the key buffer while a line is typed in */
#define KEY_HIST_UP		0x01
#define KEY_HIST_DOWN	0x02

/********************************************************************/
EXTERN	VOID	ScrollUp	( VOID						);
EXTERN	VOID	ScrollDown	( VOID						);
EXTERN	VOID	DrawCursor	( WORD x, WORD y			);
EXTERN	VOID	PutChar		( WORD x, WORD y, WORD ch	);

GLOBAL	VOID	StartBASIC	( VOID						);
GLOBAL	VOID	WaitBASIC	( VOID						);
GLOBAL	VOID	BreakBASIC	( VOID						);
GLOBAL	LONG	ContBASIC	( VOID						);

GLOBAL	WORD	Printf		( BYTE *string, ...			);
GLOBAL	WORD	FPRINTF		( FILE *fp, BYTE *string, ... );
GLOBAL	VOID	Putchar		( WORD ch					);
GLOBAL  WORD	PUTC		( WORD ch, FILE *f			);
GLOBAL  WORD	FPUTS		(char *string, FILE *f		);
GLOBAL	BYTE	*FGets		( FILE *fp, BYTE *string	);
GLOBAL	BYTE	*Gets		( BYTE *string				);
GLOBAL	VOID	peekpoke	( char *s,char *d,int bytes	);
GLOBAL	VOID	plotpoint	( int x, int y				);
GLOBAL  VOID    line		(int x0,int y0,int x1,int y1);
GLOBAL  VOID	circle		(int x0,int y0,int r		);
GLOBAL	WORD	is0byte		(char *adr					);
GLOBAL	VOID	make0byte	(char *adr					);

/********************************************************************/
#ifdef __GNUC__
/* Pure C used the NULL-safe versions with the 255 character limit
   from CPX_BIOS.S for everything. With GCC only CPX-BASIC itself gets
   them, MiNTLib keeps its own. */
#define strcpy	BAS_strcpy
#define strncpy	BAS_strncpy
#define strcat	BAS_strcat
#define strncat	BAS_strncat
#define strlen	BAS_strlen
char	*BAS_strcpy	( char *d, const char *s			);
char	*BAS_strncpy( char *d, const char *s, size_t n	);
char	*BAS_strcat	( char *d, const char *s			);
char	*BAS_strncat( char *d, const char *s, size_t n	);
size_t	BAS_strlen	( const char *s						);

/* Pure C declares these in its own headers, MiNTLib doesn't have them */
#include <ext.h>
#undef	putc		/* MiNTLib's macro: use the function from GCC/stdio.c */
#ifndef Bconout	/* not a macro from osbind.h */
void	Bconout		( int dev, int c					);
#endif
#endif

/********************************************************************/
EXTERN	MFDB	cpx_mfdb;
EXTERN	BYTE	screen[];
EXTERN	BYTE	charscreen[22][32];
extern	char	ctrl;
extern	char	ctrlsq;


#ifdef ___MAIN_C___
#undef EXTERN
#define EXTERN GLOBAL
#endif

EXTERN	WORD	roff,woff,DirtyScreen;
EXTERN	BYTE	KeyBuffer[KB_SIZE];
EXTERN  WORD	fileerror;

#ifdef ___MAIN_C___
#undef EXTERN
#define EXTERN extern
#endif

#endif
