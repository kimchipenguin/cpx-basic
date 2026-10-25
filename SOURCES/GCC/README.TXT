CPX-BASIC with the m68k-atari-mint GCC cross-compiler
=======================================================

Building
--------
(see also "The GEM application" below)

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


The GEM application (CHIPBAS)
-----------------------------
`make app` builds APP/CHIPBAS.APP (68000), APP/CHIPB030.APP
(68020+ with FPU) and APP/CHIPBV4E.APP (FireBee): the same interpreter as a GEM program with a
resizable window. APP/README.TXT is the user manual, APP/readme.rtf
the same for modern systems (python3 tools/rtf.py APP/README.TXT
APP/readme.rtf).

Line editing (both targets, CPX_IO.C FGets): cursor left/right
(KEY_CUR_LEFT/RIGHT from cpx_key/do_key), typing inserts, Backspace
deletes before, Delete under the cursor (on the CPX Delete used to be
a second Backspace), control characters are ignored. Works at the
prompt and for INPUT; the history stays prompt-only.

Built from the shared sources with -DCHIPBAS_APP (not GEMAPP: GEMlib
already defines that name):
SOURCES/GEM/gemmain.c   replaces CPX_MAIN.C: menu bar built in C,
                        window, event loop, keyboard, menu commands
SOURCES/GEM/gemscrn.c   replaces the screen part of cpx_bios.S
cpxmains.S, cpx_bios.S  the coroutine switch; only the string and
                        peekpoke routines of cpx_bios.S
GCC/stdio.c, tosglue.c  as in the CPX; stdio.c under bas_ names so
                        MiNTLib keeps its own stdio (normal startup)

How it works:
- BASIC runs as the same coroutine as in the CPX. The event loop calls
  ContBASIC() when BASIC has work (a running program, keys in the key
  buffer); BASIC gives control back after 50 ms (BreakBASIC) or when
  it waits for a key (WaitBASIC). While BASIC waits, evnt_multi()
  waits with a 100 ms timer instead of polling.
- The screen is a character grid (max_cx x max_cy, MAX_CX/MAX_CY are
  variables for CHIPBAS_APP) with a reverse flag per character, plus a
  monochrome graphics bitmap of the same size. Text is drawn with
  v_gtext in the system font, the bitmap with vrt_cpyfm; a character
  clears the graphics under its cell, graphics scroll with the text
  (as on the CPX).
- Only changed rows/columns are redrawn (the VDI is slow at text: a
  full 80-character row took ~150 ms per update on an 8 MHz ST). A
  scroll of the whole screen is a vro_cpyfm screen copy where the
  window is completely visible, else a full redraw.
- Scrollback: the top line of each full-screen scroll goes into a ring
  of 500 lines (text and reverse flags, trailing spaces cut, no
  graphics). `view' = lines scrolled back; window row r shows grid row
  r - view, or a scrollback line if that is negative. While the user
  looks back, new output keeps the view where it is. Slider: WM_VSLID,
  WM_ARROWED, Shift+Up/Down; any other key goes back to view 0.
  Shrinking the window pushes the lines above the cursor into the
  scrollback, growing it pulls them back if the text reached the
  bottom.
- Iconify (SMALLER) where appl_getinfo(AES_WINDOW) says so; the
  iconified window shows the CPX icon (from cpxhead.bin) and "BASIC",
  BASIC keeps running. The size before iconify is remembered; while
  iconified the event loop also takes double clicks (MU_BUTTON) and
  uniconifies itself, as not every AES sends WM_UNICONIFY for them.
  WM_UNICONIFY with a rectangle of a different size only gives the
  position.
- Menu commands are typed into the key buffer as BASIC commands
  (KEY_LINE_KILL first clears the line typed so far), so they only
  work at the prompt; the menu greys them out otherwise.
- Ctrl+Q quits (with an alert if a program is in memory), Ctrl+S
  stops/continues output while BASIC isn't at the prompt.
- exit() of the BASIC is basic_exit() now (P2C.H), so it doesn't
  clash with MiNTLib's exit() (CPX unchanged).

Tested in Hatari: TOS 1.04 (ST), TOS 2.06 (Mega STE, ST High and ST
Low colour), TOS 3.06 (TT High, CHIPB030 with 68882). TOS 1.62 with
autostart via DESKTOP.INF #Z crashed even for a minimal GEM program,
so it wasn't tested there. Not tested: MagiC, MultiTOS, Falcon,
graphics cards, real hardware.

Test: test/hatari.sh <name> APP/CHIPBAS.APP <script> starts the
program via DESKTOP.INF (#Z) instead of XControl; use TOS 2.06 and
KEYMAP=de, e.g.
  TOS=".../TOS v2.06 ... (DE).img" MACHINE=megaste KEYMAP=de \
    test/hatari.sh app1 APP/CHIPBAS.APP test/scripts/app1.txt

FireBee (CHIPBV4E, -mcpu=5475): cpxmains.S and cpx_bios.S use 68000
instructions ColdFire lacks (movem -(sp)/(sp)+ or absolute, dbra/dbeq,
exg, word arithmetic). That target uses GCC/cfcoro.S (context switch
saving only d2-d7/a2-a6 and sp through a0) and GCC/cfglue.c
(BreakBASIC/ContBASIC/WaitBASIC/basic_exit, BAS_str*, is0byte,
make0byte, peekpoke in C) instead; the 68000 builds keep the
assembler. Checked: no 68000-only instruction in the objects
(objdump -m m68k:cfv4e), and a 68000 build with cfcoro.S/cfglue.c
gives the same results as the normal one in Hatari. Not run on a
FireBee (no ColdFire emulator here).

Window position: saved to CHIPBAS.INF in the start folder on quit
(screen size + outer rectangle), used at the next start if the screen
size is the same, clamped to the desktop and snapped to whole
characters.

Open: the manual window (README in a GEM window).
