|------------------------------------------------------------------
|
|	Basic-Input/Output (BIOS) - GCC version of CPX_BIOS.S
|
|------------------------------------------------------------------
|
| If EXTRAFONT is set to a value != 0, the own 8x8 font is used
| instead of the system font, i.e. no LINE-A, but longer.
|
| The routines are the Pure C ones, which take their arguments in
| registers (d0-d2, a0-a1). Each one called from C got an entry stub
| (_name) that takes the GCC stack arguments (32-bit int) and keeps
| d2/a2, which GCC expects to survive a call.
|
| The string functions are exported as BAS_strcpy etc.; glo.h maps the
| C names onto them, so MiNTLib keeps its own versions.
|
| With CHIPBAS_APP (the GEM application) only the string functions and
| peekpoke are assembled; the screen is GEM/gemscrn.c there.
|------------------------------------------------------------------

	.text

NULL_KENN = 0x87916543		| 4-byte marker

|------------------------------------------------------------------
| Finds out whether a preceding zero byte terminates the string
| int is0byte(char *);
|------------------------------------------------------------------
	.globl	_is0byte
_is0byte:
	move.l	4(%sp),%a0
	moveq	#0,%d0
	bsr	is0byte__
	bne.s	1f
	moveq	#4,%d0
1:	rts

is0byte__:
	cmp.b	#(NULL_KENN>>24)&0xff,(%a0)
	bne.s	1f
	cmp.b	#(NULL_KENN>>16)&0xff,1(%a0)
	bne.s	1f
	cmp.b	#(NULL_KENN>>8)&0xff,2(%a0)
	bne.s	1f
	cmp.b	#NULL_KENN&0xff,3(%a0)
1:	rts

|------------------------------------------------------------------
| Puts a marker behind a zero byte
| void make0byte(char *);
|------------------------------------------------------------------
	.globl	_make0byte
_make0byte:
	move.l	4(%sp),%a0
	move.b	#(NULL_KENN>>24)&0xff,(%a0)
	move.b	#(NULL_KENN>>16)&0xff,1(%a0)
	move.b	#(NULL_KENN>>8)&0xff,2(%a0)
	move.b	#NULL_KENN&0xff,3(%a0)
	clr.b	4(%a0)
	rts

|------------------------------------------------------------------
| Own versions of the C standards: catch NULL pointers, copy at most
| 255 characters, respect the NULL marker
|   a0 = dest, a1 = source, d0.w = count; result in a0 (d0 for strlen)
|------------------------------------------------------------------
	.globl	_BAS_strcpy,_BAS_strncpy,_BAS_strcat,_BAS_strncat,_BAS_strlen

_BAS_strcpy:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%a0
	move.l	12(%sp),%a1
	bsr	strcpy_
	bra	str_ret

_BAS_strncpy:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%a0
	move.l	12(%sp),%a1
	move.l	16(%sp),%d0
	bsr	strncpy_
	bra	str_ret

_BAS_strcat:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%a0
	move.l	12(%sp),%a1
	bsr	strcat_
	bra	str_ret

_BAS_strncat:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%a0
	move.l	12(%sp),%a1
	move.l	16(%sp),%d0
	bsr	strncat_
str_ret:
	move.l	%a0,%d0
	move.l	(%sp)+,%d2
	rts

_BAS_strlen:
	move.l	4(%sp),%a0
	bra	strlen_

strcpy_:
	move.w	#254,%d0
strncpy_:
	move.l	%a0,%d2
	beq.s	bye_strcpy
entry:	move.l	%a1,%d1
	beq.s	bye_strcpy
	bra	2f
1:	move.b	(%a1)+,(%a0)+
2:	dbeq	%d0,1b
	exg	%a0,%a1
	bsr	is0byte__
	exg	%a0,%a1
	dbne	%d0,1b
	clr.b	(%a0)
	move.l	%d2,%a0
bye_strcpy:
	rts

strcat_:
	move.w	#254,%d0
strncat_:
	move.l	%a0,%d2
	beq.s	bye_strcpy
1:	tst.b	(%a0)+
	bne.s	1b
	bsr	is0byte__
	beq.s	1b
	subq.l	#1,%a0
	bra	entry

strlen_:
	move.l	%a0,%d0
	beq.s	3f
	moveq	#-1,%d0
1:	tst.b	(%a0)+
	dbeq	%d0,1b
	bsr	is0byte__
	dbne	%d0,1b
	eor.l	#-1,%d0
3:	rts

#ifndef CHIPBAS_APP
|------------------------------------------------------------------
|  Draws a line with the Bresenham method
|  taken from:
|  David Kastrup, ''Einfache Kurven auf Rastergrafiken''
|  plus handling of the different slopes
|  void line(int x1,int y1,int x2,int y2);
|------------------------------------------------------------------
	.globl	_line
_line:
	movem.l	%d2-%d7/%a2,-(%sp)	| 7 registers = 28 bytes
	move.l	32(%sp),%d0		| x0
	move.l	36(%sp),%d1		| y0
	move.l	40(%sp),%d2		| x1
	move.w	46(%sp),-(%sp)		| y1 as the Pure C stack argument
	bsr	line_
	addq.l	#2,%sp
	movem.l	(%sp)+,%d2-%d7/%a2
	rts

| D0.w: x0
| D1.w: y0
| D2.w: x1
| 4(sp): y1
|
| D3: y1
| D4: a
| D5: b
| D6: step
| D7: inc
| (sp): x
| 2(sp): y
line_:	movem.l	%d3-%d7/%a2,-(%sp)	| 6 registers
	move.w	4+6*4(%sp),%d3		| y1
	lea	-4(%sp),%a1		| x
	lea	-2(%sp),%a2		| y
	move.w	%d3,%d4
	sub.w	%d1,%d4			| a=y1-y0
	move.w	%d4,%d6
	bpl.s	1f
	neg.w	%d6			| abs(a)
1:	move.w	%d2,%d5
	sub.w	%d0,%d5			| b=x1-x0
	move.w	%d5,%d7
	bpl.s	2f
	neg.w	%d7			| abs(b)
2:	cmp.w	%d6,%d7			| abs(a)<abs(b)
	bhs.s	3f
	exg	%a1,%a2			| xp<->yp
	exg	%d0,%d1			| x0<->y0
	exg	%d2,%d3			| x1<->y1
	exg	%d4,%d5			| a <->b
3:	movem.w	%d0-%d1,-(%sp)		| x=x0,y=y0
	tst.w	%d5			| b<0
	bpl.s	4f
	neg.w	%d5
	neg.w	%d4
	move.w	%d2,(%sp)		| x=x1
	move.w	%d3,2(%sp)		| y=y1
	move.w	%d0,%d2			| x1=x0
4:	move.w	%d5,%d6
	lsr.w	#1,%d6			| step=b/2
	sub.w	(%sp),%d2		| number of points to draw
	move.w	%d2,%d3			| saves saving across plotpoint
	moveq	#-1,%d7			| inc=-1
	tst.w	%d4			| a>0
	ble.s	5f
	neg.w	%d4			| a: always negative or 0!
	moveq	#1,%d7
5:	move.w	(%a1),%d0
	move.w	(%a2),%d1
	bsr	plotpoint_
	addq.w	#1,(%sp)		| x++
	add.w	%d4,%d6			| step-=a
	bgt.s	6f			| step<=0
	add.w	%d7,2(%sp)		| y+=inc
	add.w	%d5,%d6			| step+=b
6:	dbra	%d3,5b			| x>x1
	addq.l	#4,%sp
	movem.l	(%sp)+,%d3-%d7/%a2
	rts

|------------------------------------------------------------------
| Plots a point at the given coordinate
| void plotpoint(int x,int y)
|------------------------------------------------------------------
	.globl	_plotpoint
_plotpoint:
	move.l	%d2,-(%sp)
	move.l	8(%sp),%d0
	move.l	12(%sp),%d1
	bsr	plotpoint_
	move.l	(%sp)+,%d2
	rts

| D0.w x
| D1.w y
plotpoint_:
	cmp.w	#256,%d0		| catch over- and underflow
	bhs.s	1f
	cmp.w	#176,%d1
	bhs.s	1f
	neg.w	%d1			| lower left corner is 0
	add.w	#175,%d1
	lsl.w	#5,%d1			| y in bytes
	move.w	%d0,%d2			| keep x
	lsr.w	#3,%d0			| byte offset in x
	add.w	%d0,%d1			| total offset
	and.w	#7,%d2			| pixel offset in the byte
	eor.w	#7,%d2			| bit number
	lea	_screen,%a0
	bset	%d2,0(%a0,%d1.w)	| set the point
	st	_DirtyScreen
1:	rts

#endif /* !CHIPBAS_APP */

|------------------------------------------------------------------
| Moves the given number of bytes from source to dest
| (called in supervisor mode via Supexec)
| void peekpoke(char *source, char *dest, int bytes);
|------------------------------------------------------------------
	.globl	_peekpoke
_peekpoke:
	move.l	4(%sp),%a0		| source
	move.l	8(%sp),%a1		| destination
	move.l	12(%sp),%d0		| number of bytes
	move.w	%sr,%d1
	ori.w	#0x0700,%sr
	subq.w	#1,%d0
1:	move.b	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.w	%d1,%sr
	rts

#ifndef CHIPBAS_APP
GetScreen:
	lsl.w	#8,%d0
	lea	_screen,%a0
	lea	(%a0,%d0.w),%a0
	lea	256(%a0),%a1
	neg.w	%d0
	add.w	#21*32*8,%d0
	rts

GetCharScreen:
	lsl.w	#5,%d0
	lea	_charscreen,%a0
	lea	(%a0,%d0.w),%a0
	lea	32(%a0),%a1
	neg.w	%d0
	add.w	#32*21,%d0
	rts

|------------------------------------------------------------------
| void ScrollUp(void), void ScrollUpR(int line)
|------------------------------------------------------------------
	.globl	_ScrollUp,_ScrollUpR
_ScrollUp:
	moveq	#0,%d0
	bra	1f
_ScrollUpR:
	move.l	4(%sp),%d0
1:	movem.l	%d2/%a2,-(%sp)
	bsr	ScrollUpR_
	movem.l	(%sp)+,%d2/%a2
	rts

ScrollUpR_:
	move.w	%d0,-(%sp)
	bsr	GetScreen
	bsr	scr_cpy_up

	move.w	(%sp)+,%d0
	bsr	GetCharScreen
	bsr	scr_cpy_up

	moveq	#21,%d0
	bsr	clline_

	jmp	_do_draw

scr_cpy_up:
	lsr.w	#2,%d0
	bra	2f
1:	move.l	(%a1)+,(%a0)+
2:	dbra	%d0,1b
	rts

|------------------------------------------------------------------
| void ScrollDown(void), void ScrollDownR(int line)
|------------------------------------------------------------------
	.globl	_ScrollDown,_ScrollDownR
_ScrollDown:
	moveq	#0,%d0
	bra	1f
_ScrollDownR:
	move.l	4(%sp),%d0
1:	movem.l	%d2/%a2,-(%sp)
	bsr	ScrollDownR_
	movem.l	(%sp)+,%d2/%a2
	rts

ScrollDownR_:
	move.w	%d0,-(%sp)
	bsr	GetScreen
	bsr	scr_cpy_d

	move.w	(%sp),%d0
	bsr	GetCharScreen
	bsr	scr_cpy_d

	move.w	(%sp)+,%d0
	bsr	clline_

	jmp	_do_draw

scr_cpy_d:
	lea	(%a0,%d0.w),%a0
	lea	(%a1,%d0.w),%a1
	lsr.w	#2,%d0
	bra	2f
1:	move.l	-(%a0),-(%a1)
2:	dbra	%d0,1b
	rts

|------------------------------------------------------------------
|	VOID DrawCursor(WORD x, WORD y)
|------------------------------------------------------------------
	.globl	_DrawCursor
_DrawCursor:
	move.l	4(%sp),%d0		| x
	move.l	8(%sp),%d1		| y
	lea	_screen,%a0
	lsl.w	#8,%d1
	add.w	%d1,%d0
	lea	(%a0,%d0.w),%a0		| address of the cursor on the screen

	not.b	(%a0)
	not.b	32(%a0)
	not.b	64(%a0)
	not.b	96(%a0)			| draw/erase the cursor
	not.b	128(%a0)
	not.b	160(%a0)
	not.b	192(%a0)
	not.b	224(%a0)
	st	_DirtyScreen
	rts

|--------------------------------------------------------------------
|	void cls(void)			: clear the whole screen
|	void clline(int line)		: clear a line
|	void cllines(int from, int to)	: clear several lines
|	void delxy(int line, int from_x, int to_x) : clear within a line
|--------------------------------------------------------------------
	.globl	_cls,_clline,_cllines,_delxy
_cls:
	movem.l	%d2/%a2,-(%sp)
	bsr	cls_
	bra	cl_ret
_cllines:
	movem.l	%d2/%a2,-(%sp)
	move.l	12(%sp),%d0
	move.l	16(%sp),%d1
	bsr	cllines_
	bra	cl_ret
_clline:
	movem.l	%d2/%a2,-(%sp)
	move.l	12(%sp),%d0
	bsr	clline_
	bra	cl_ret
_delxy:
	movem.l	%d2/%a2,-(%sp)
	move.l	12(%sp),%d0
	move.l	16(%sp),%d1
	move.l	20(%sp),%d2
	bsr	delxy_
cl_ret:
	movem.l	(%sp)+,%d2/%a2
	rts

cls_:	moveq	#0,%d0
	moveq	#21,%d1

| D0: first line
| D1: last line
cllines_:
	movem.l	%d3/%d4,-(%sp)
	move.w	%d0,%d3
	move.w	%d1,%d4
	bra	2f
1:	move.w	%d3,%d0
	bsr	clline_
	addq.w	#1,%d3
2:	cmp.w	%d4,%d3
	ble.s	1b
	movem.l	(%sp)+,%d3/%d4
	rts

| d0: line
clline_:
	moveq	#0,%d1
	moveq	#31,%d2

| d0: line
| d1: first x
| d2: last x
delxy_:
	movem.l	%d3-%d5,-(%sp)
	move.w	_rev_mode,-(%sp)
	clr.b	_rev_mode
	move.w	%d0,%d3
	move.w	%d1,%d4
	move.w	%d2,%d5
	bra	2f
1:	move.w	%d4,%d0
	move.w	%d3,%d1
	moveq	#' ',%d2
	bsr	PutChar_
	addq.w	#1,%d4
2:	cmp.w	%d5,%d4
	ble.s	1b
	move.w	(%sp)+,_rev_mode
	movem.l	(%sp)+,%d3-%d5
	rts

|------------------------------------------------------------------
|	VOID PutChar(WORD x, WORD y, WORD ch);
|------------------------------------------------------------------
	.globl	_PutChar
_PutChar:
	movem.l	%d2/%a2,-(%sp)
	move.l	12(%sp),%d0		| x
	move.l	16(%sp),%d1		| y
	move.l	20(%sp),%d2		| character
	bsr	PutChar_
	movem.l	(%sp)+,%d2/%a2
	rts

| d0.w -> x
| d1.w -> y
| d2.w -> character
PutChar_:
	tst.w	_ctrl			| checks Ctrl-C and Ctrl-Q
	bne.s	1f			| at the same time
	movem.w	%d0-%d2,-(%sp)
	jsr	_WaitBASIC
	movem.w	(%sp)+,%d0-%d2
	bra	PutChar_

1:	cmp.w	#31,%d0
	bhi	9f
	cmp.w	#21,%d1
	bhi	9f
	cmp.w	#255,%d2
	bhi	9f
	lea	_charscreen,%a0		| remember the character
	lsl.w	#5,%d1			| 32 bytes per line
	add.w	%d0,%d1			| plus x
	move.b	%d2,0(%a0,%d1.w)	| and remember the character
	sub.w	%d0,%d1			| subtract x again

	lsl.w	#3,%d1			| 32 bytes per line * 8 lines
	add.w	%d1,%d0			| total offset in the screen

#if EXTRAFONT
	lea	_font8x8,%a1
	lsl.w	#3,%d2
	add.w	%d2,%a1			| address of the character in the font
	moveq	#7,%d1			| 8 pixels high
#else
	lea	font_d,%a0
	tst.l	(%a0)
	bne.s	2f			| already initialised?

	movem.l	%d0-%d2/%a0/%a2,-(%sp)	| initialise
	.word	0xa000			| LINE-A init
	movem.l	(%sp)+,%d0-%d2/%a0/%a2
	move.l	4(%a1),%a1		| 8x8 system font
	move.w	0x52(%a1),%d1
	subq.w	#1,%d1			| height of the font
	move.w	%d1,font_h-font_d(%a0)
	move.w	0x50(%a1),font_w-font_d(%a0) | width of the font
	move.l	0x4c(%a1),(%a0)		| the font data itself

2:	move.l	(%a0),%a1
	add.w	%d2,%a1			| address of the character in the font
	move.w	font_h-font_d(%a0),%d1
	move.w	font_w-font_d(%a0),%d2
#endif

	lea	_screen,%a0
	add.w	%d0,%a0			| address of the character on the screen

	move.b	_rev_mode,%d0

3:
#if EXTRAFONT
	move.b	(%a1)+,(%a0)
#else
	move.b	(%a1),(%a0)
	add.w	%d2,%a1
#endif
	eor.b	%d0,(%a0)		| copy the font data
	lea	32(%a0),%a0
	dbra	%d1,3b
	st	_DirtyScreen
9:	rts

|------------------------------------------------------------------
	.data
	.even
	.globl	_cpx_mfdb
_cpx_mfdb:
	.long	_screen
	.word	256
	.word	176
	.word	16
	.word	1
	.word	1
	.word	0,0,0		| reserved

	.bss
	.even
#if !EXTRAFONT
font_h:	.skip	2
font_w:	.skip	2
font_d:	.skip	4
#endif

	.globl	_rev_mode,_screen,_charscreen
_rev_mode:
	.skip	1
	.skip	1

_screen:
	.skip	(256/8)*176	| screen memory

_charscreen:
	.skip	32*22		| memory for all characters
#endif /* !CHIPBAS_APP */
