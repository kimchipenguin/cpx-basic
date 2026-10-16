#
# Makefile for CPX-BASIC - m68k-atari-mint GCC cross-compiler
#
#   make            build all four CPX modules into CPX/
#   make BASIC000   68000, own 8x8 font             -> CPX/BASIC000.CPX
#   make BASIC030   68020+ with FPU, own 8x8 font   -> CPX/BASIC030.CPX
#   make BASICA00   68000, system font (LINE-A)     -> CPX/BASICA00.CPX
#   make BASICA30   68020+ with FPU, system font    -> CPX/BASICA30.CPX
#   make clean
#
# Replaces the Pure C projects SOURCES/BASIC*.PRJ and BASBUILD.PRG.
# The GCC-only parts are in SOURCES/GCC: the assembler files in GNU as
# syntax, the CPX entry with the XControl glue, malloc() with
# _FreeAll(), the Pure C library functions MiNTLib lacks and the
# headers TOS.H, PORTAB.H, AES.H, VDI.H, EXT.H.
#

CROSS   = m68k-atari-mint-
CC      = $(CROSS)gcc
STRIP   = $(CROSS)strip
PYTHON  = python3

# CPX version (CPX header; the start-up text is in BASIC.C)
VERSION = 0x130

SRC     = SOURCES
GCCDIR  = $(SRC)/GCC
OBJDIR  = build
CPXDIR  = CPX

# Pure C's int is 16 bits, but MiNTLib has no -mshort libc. So int is
# 32 bits here and the XControl interface is adapted in GCC/cpxstart.S.
# The sources are old K&R-ish C from p2c, hence gnu89. -funsigned-char
# is Pure C's -K from the .PRJ files.
CFLAGS  = -std=gnu89 -O2 -fomit-frame-pointer -funsigned-char -DNDEBUG \
          -I$(GCCDIR)/include -I$(SRC) \
          -Wall -Wno-multichar -Wno-unused-variable -Wno-unused-function \
          -Wno-unused-but-set-variable -Wno-parentheses \
          -Wno-unknown-pragmas -Wno-endif-labels -Wno-implicit-int
ASFLAGS =
# no MiNTLib startup code: XControl calls cpx_init() directly
LDFLAGS = -nostartfiles
LIBS    = -lgem -lm

CSRCS   = $(SRC)/CPX_MAIN.C $(SRC)/BASIC.C $(SRC)/CPX_IO.C $(SRC)/TOKSTR.C \
          $(SRC)/P2CLIB.C $(GCCDIR)/alloc.c $(GCCDIR)/tosglue.c $(GCCDIR)/stdio.c
HDRS    = $(SRC)/GLO.H $(SRC)/P2C.H $(SRC)/PROTO.H $(SRC)/TOKSTR.H \
          $(SRC)/XCONTROL.H $(wildcard $(GCCDIR)/include/*.h)

VARIANTS = BASIC000 BASIC030 BASICA00 BASICA30

CPU_BASIC000 = -m68000
CPU_BASIC030 = -m68020-60 -DTT_VERS
CPU_BASICA00 = -m68000
CPU_BASICA30 = -m68020-60 -DTT_VERS
FONT_BASIC000 = 1
FONT_BASIC030 = 1
FONT_BASICA00 = 0
FONT_BASICA30 = 0

all: $(VARIANTS)

# cpxstart.o has to come first: it is the start of the text segment
define VARIANT_RULES
$(1): $(CPXDIR)/$(1).CPX

$(1)_OBJS = $(OBJDIR)/$(1)/cpxstart.o \
            $(patsubst %,$(OBJDIR)/$(1)/%.o,$(basename $(notdir $(CSRCS)))) \
            $(OBJDIR)/$(1)/cpxmains.o $(OBJDIR)/$(1)/cpx_bios.o \
            $(if $(filter 1,$(FONT_$(1))),$(OBJDIR)/$(1)/font8x8.o)

$(OBJDIR)/$(1)/%.o: $(SRC)/%.C $(HDRS)
	@mkdir -p $$(@D)
	$(CC) $(CPU_$(1)) $(CFLAGS) -c -o $$@ -x c $$<

# These implement C library functions: without -fno-builtin GCC turns
# malloc() + memset() in calloc() into a call to calloc() itself.
$(OBJDIR)/$(1)/%.o: $(GCCDIR)/%.c $(HDRS)
	@mkdir -p $$(@D)
	$(CC) $(CPU_$(1)) $(CFLAGS) -fno-builtin -c -o $$@ $$<

$(OBJDIR)/$(1)/%.o: $(GCCDIR)/%.S
	@mkdir -p $$(@D)
	$(CC) $(CPU_$(1)) $(ASFLAGS) -DEXTRAFONT=$(FONT_$(1)) -c -o $$@ $$<

$(OBJDIR)/$(1)/font8x8.o: $(OBJDIR)/font8x8.s
	@mkdir -p $$(@D)
	$(CC) $(CPU_$(1)) -c -o $$@ $$<

$(OBJDIR)/$(1)/$(1).PRG: $$($(1)_OBJS)
	$(CC) $(CPU_$(1)) $(LDFLAGS) -o $$@ $$^ $(LIBS)
	$(STRIP) $$@

$(CPXDIR)/$(1).CPX: $(OBJDIR)/$(1)/$(1).PRG $(GCCDIR)/cpxhead.bin $(GCCDIR)/mkcpx.py
	@mkdir -p $$(@D)
	$(PYTHON) $(GCCDIR)/mkcpx.py $(GCCDIR)/cpxhead.bin $$< $$@ $(VERSION)
endef

$(foreach v,$(VARIANTS),$(eval $(call VARIANT_RULES,$(v))))

# The 8x8 font is converted from the Pure C assembler source
$(OBJDIR)/font8x8.s: $(SRC)/FONT8X8.S
	@mkdir -p $(@D)
	LC_ALL=C perl -ne 's/\r//; s/;.*//; \
	    if (/^\s*font8x8:/) { print "\t.data\n\t.globl\t_font8x8\n_font8x8:\n" } \
	    elsif (/^\s*\.dc\.b\s+%([01]+)/) { print "\t.byte\t0b$$1\n" }' $< > $@

clean:
	rm -rf $(OBJDIR)

.PHONY: all clean $(VARIANTS)
