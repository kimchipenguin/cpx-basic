Chipmunk BASIC for GEM
======================

Chipmunk BASIC 1.35 as a GEM program
with a resizable window, based on the
control panel module CPX-BASIC. The
interpreter is the same as in the CPX,
with all its commands and functions.

Requirements
------------

Any Atari ST, STE, TT or Falcon with
TOS 1.04 or later, any resolution and
colour depth with at least 20 x 5
characters.

CHIPBAS.APP runs everywhere.
CHIPB030.APP needs a 68020 or better
with an FPU (TT, Falcon with FPU) and
is faster there. CHIPBV4E.APP is the
version for the FireBee.

Controls
--------

Menu commands
  Ctrl+N      New (deletes the program)
  Ctrl+O      Open... (LOAD, file
              selector)
  Ctrl+M      Merge... (MERGE)
  Ctrl+S      Save... (SAVE)
  Ctrl+Q      Quit
  Ctrl+R      Run
  Ctrl+C      Stop a running program
  Ctrl+L      List
  Help        List of all keywords

While a program runs
  Ctrl+S      Stop the output, Ctrl+S
              again goes on
  Ctrl+C      Stop (CONTINUE goes on)

Typing a line (also for INPUT)
  Left/Right  Move the cursor in the
              line; typing inserts
  Backspace   Delete before the cursor
  Delete      Delete under the cursor
  Up/Down     The last ten lines typed
              in (only at the > prompt)

Older output
  Shift+Up    One line further back
  Shift+Down  One line forward
  Any key     Back to the screen

The commands of the File and Program
menus can only be used at the >
prompt; Stop only while a program runs.

The window
----------

The text area follows the window size.
Make the window bigger for longer lines
or more of them. PLOT, LINE and CIRCLE
draw in the same area, (0,0) is the
lower left corner. Graphics scroll up
together with the text, and characters
cover the graphics under them.

The last 500 lines that scrolled off
the top are kept (text only, without
graphics). The slider and the arrows
on the right show them. When the
window gets smaller, the lines above go
there instead of being lost, and come
back when it grows again.

Where the system has it (MagiC,
MultiTOS, XaAES), the window can be
iconified; the program goes on running.

The window's place and size are saved
in CHIPBAS.INF next to the program when
you quit, and used again at the next
start (in the same screen resolution).

Files are loaded from and saved in the
current folder, unless a path is given.
LOAD, MERGE and SAVE without a name and
RUN "" open the file selector.

Additions to CPX-BASIC
----------------------

- GEM window of any size instead of
  the 32 x 22 characters of the CPX
- System font instead of the 8x8 font
- Menu bar with keyboard shortcuts
- The program asks before quitting
  when a program is in memory
- The last 500 lines of output with a
  slider, iconify
- Remembers the window position
- A version for the FireBee
- Cursor left/right and Delete in the
  line being typed (also in the CPX)

Differences to CPX-BASIC
------------------------

- Ctrl+Q quits; on the CPX it typed
  BYE. Ctrl+S stops and continues the
  output; on the CPX Ctrl+Q went on.
- The Ctrl keys that typed keywords on
  the CPX (Ctrl+F for FOR, ...) are
  not there, apart from the menu
  shortcuts.
- Clicking characters with the mouse
  does not type them.

Notes
-----

Chipmunk BASIC by David Gillespie,
CPX-BASIC by Oliver Teuber, Jan
Starzynski and Mia Jaap. Freeware under
the GNU General Public License.

Ported using Claude AI tools, design by
Kimchipenguin (Mia)
