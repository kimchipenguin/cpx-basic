CPX-BASIC with the m68k-atari-mint GCC cross-compiler
=======================================================

Building
--------
In the project folder (the one with the Makefile):

    make            all four modules -> CPX/BASIC000.CPX, BASIC030.CPX,
                                        BASICA00.CPX, BASICA30.CPX
    make BASIC000   just one of them
    make clean      removes build/

Needs m68k-atari-mint-gcc (MiNTLib, GEMlib) in the PATH and python3.
This replaces the Pure C projects (BASIC*.PRJ) and BASBUILD.PRG; the
Pure C files are still there and unchanged in what they build.

    BASIC000   68000, own 8x8 font
    BASIC030   68020+ with FPU (TT, Falcon with FPU), own 8x8 font
    BASICA00   68000, system font via LINE-A
    BASICA30   68020+ with FPU, system font via LINE-A


What is in this folder
----------------------
cpxstart.S   CPX entry (__start -> cpx_init) and the XControl glue
cpxmains.S   coroutine switching between CPX and BASIC (was CPXMAINS.S)
cpx_bios.S   screen, font, graphics and string routines (was CPX_BIOS.S)
alloc.c      malloc()/free()/realloc() with _FreeAll()
stdio.c      fopen() & co. directly on GEMDOS, with Pure C's text mode
tosglue.c    Random(), Supexec(), Bconout() ... as functions, ltoa()
include/     TOS.H, PORTAB.H, AES.H, VDI.H, EXT.H for GCC
cpxhead.bin  the 512-byte CPX header (icon, texts), from the Pure C CPX
mkcpx.py     header + program -> .CPX (does what BASBUILD did)

FONT8X8.S is converted to GNU as syntax by the Makefile at build time.


Differences to the Pure C build
-------------------------------
- int is 32 bits (MiNTLib has no 16-bit int libc). XControl passes
  and expects 16-bit WORDs, so the callbacks with WORD arguments go
  through thunks in cpxstart.S, and the XCPB functions are called
  through xcall(), which also keeps the registers GCC needs.
- The assembler routines took their arguments in registers (Pure C).
  Each one has a GCC entry stub now; the routines themselves are the
  same.
- The NULL-safe 255-character strcpy() & co. from CPX_BIOS.S are
  BAS_strcpy() etc.; GLO.H maps the names for CPX-BASIC's own code,
  MiNTLib keeps its own versions.
- -funsigned-char, as Pure C's -K in the .PRJ files (otherwise
  characters >= 128 are not printed).
- The C files in this folder are built with -fno-builtin: they
  implement C library functions, and GCC would otherwise turn
  malloc() + memset() in calloc() into a call to calloc() itself.
- double is 64 bits (Pure C: 80 bits), so the last digits of results
  can differ.
- The CPX files are bigger, mostly because of MiNTLib's exact printf()
  number formatting and the libm functions.

- MiNTLib's iscntrl() is ISO 8859-1 (128-159 are control codes);
  CPX_IO.C uses Pure C's meaning (0-31, 127) so that the Atari
  umlauts and the pound sign are printed.

Bugs fixed that were in the Pure C build as well:
- CPX_MAIN.C, BASIC030/BASICA30: XControl's getcookie() returns != 0
  when the cookie is found; the CPU/FPU check took that as an error,
  so the 68030 modules always said "CPU oder FPU nicht vorhanden".
- BASIC.C, LOWER$/UPPER$: strlen(TEMPS[i]) on a character crashed
  the Pure C module (reset in the emulator).
- BASIC.C, numtostr(): the zeros of the exponent were removed from
  the first '0' on, so with GCC's two exponent digits 1E10 came out
  as 1E+1 (and with Pure C 1E100 as 1E+1).

New functions and commands (2026-10-16, BASIC.C, TOKSTR.H/.C):
  INSTR(h$,n$)  position of n$ in h$ (1 = first character), 0 if absent
  REPLACE$(s$,old$,new$)  all occurrences of old$ replaced (max. 255)
  EXISTS(f$)    1 if the file or folder exists (Fattrib), else 0
  TRIM$(s$), LTRIM$(s$), RTRIM$(s$)  remove spaces on both/left/right
  REPEAT$(n,s$) s$ n times (max. 255 characters)
  COOKIE(s$)    value of cookie s$ (e.g. "_CPU"), 0 if not in the jar
  SLEEP n       wait n ms (5 ms steps); XControl keeps running, Ctrl-C
                stops the wait
The new names are reserved words now, so they can't be variables.

File selector (BASIC.C, FileSelect()): LOAD, MERGE and SAVE without a
name and RUN "" open the file selector (*.BAS); Cancel does nothing.
FSEL$ uses the same function, so all of them remember the folder.

Direct mode history (CPX_MAIN.C, CPX_IO.C): cursor up/down at the
">" prompt cycle through the last ten lines typed in (empty lines and
repeats of the line before are not stored). cpx_key() puts the cursor
keys into the key buffer as KEY_HIST_UP/DOWN (GLO.H) while a line is
being typed; INPUT ignores them.

Changes in the shared sources (all also fine for Pure C):
- CPX_MAIN.C: GCC block for the XControl glue, WORD instead of int
  where AES/VDI/XControl data is meant.
- BASIC.C: the fixes above; fsel_input() gets a WORD; forward
  declaration of struct LOC_exec for PROTO.H.
- PROTO.H: prototypes updated to the functions in BASIC.C; a line
  break was missing, which hid the listtokens() prototype.
- GLO.H: GCC block (string function names, ltoa/ultoa, Bconout).
- TOKSTR.C: the token count check without sizeof in #if for GCC.
- P2C.H, PROTO.H: trailing ^Z removed.
The originals of the changed files are in test/orig-src.
