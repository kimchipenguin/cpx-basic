|
| cpxmains.S - GCC version of CPXMAINS.S
|
| The BASIC interpreter runs as a coroutine on its own stack. The CPX
| timer starts or continues it (StartBASIC, ContBASIC), BASIC gives
| control back with WaitBASIC or, every 50 ms, with BreakBASIC.
| exit() ends BASIC and returns 'STOP' to the CPX side.
|
| Unlike the Pure C version, BreakBASIC and ContBASIC keep d2/a2 around
| the TOS traps: GCC expects them to survive a function call.
|

	.text

|-------------------------------------------------------------------
| void exit(int)  -  BASIC ends: back to the CPX, return 'STOP'
|-------------------------------------------------------------------
	.globl	_exit
_exit:
	movem.l	%d0-%d7/%a0-%a7,regBASIC
	movem.l	regCPX,%d0-%d7/%a0-%a7
	move.l	#0x53544F50,%d0		| 'STOP'
	rts

|-------------------------------------------------------------------
| void StartBASIC(void)
|-------------------------------------------------------------------
	.globl	_StartBASIC
_StartBASIC:
	movem.l	%d0-%d7/%a0-%a7,regCPX
	movem.l	regBASIC,%d0-%d7/%a0-%a7
	move.l	#_exit,-(%sp)		| basic() returns into exit
	st	_ctrlsq
	jmp	_basic

|-------------------------------------------------------------------
| void BreakBASIC(void)  -  switch to the CPX if a key is waiting or
|                           50 ms have passed since ContBASIC
|-------------------------------------------------------------------
	.globl	_BreakBASIC
_BreakBASIC:
	movem.l	%d2/%a2,-(%sp)
	move.w	#2,-(%sp)		| CON:
	move.w	#1,-(%sp)		| Bconstat
	trap	#13
	addq.l	#4,%sp
	tst.w	%d0
	bne.s	1f			| key pressed: switch
	bsr.s	clock			| get system time
	move.l	systime,%d1
	cmp.l	%d1,%d0			| sanity check:
	bcs.s	1f			| time doesn't run backwards!
	add.l	#10,%d1			| 50 ms on top
	cmp.l	%d1,%d0			| time over?
	bcs.s	2f			| no
1:	movem.l	(%sp)+,%d2/%a2
	bra.s	_WaitBASIC
2:	movem.l	(%sp)+,%d2/%a2
	rts

|-------------------------------------------------------------------
| void WaitBASIC(void)  -  context switch to the CPX
|-------------------------------------------------------------------
	.globl	_WaitBASIC
_WaitBASIC:
	movem.l	%d0-%d7/%a0-%a7,regBASIC
	movem.l	regCPX,%d0-%d7/%a0-%a7
	moveq	#0,%d0			| ContBASIC result: not 'STOP'
	rts

|-------------------------------------------------------------------
| long ContBASIC(void)  -  context switch to BASIC
|-------------------------------------------------------------------
	.globl	_ContBASIC
_ContBASIC:
	movem.l	%d0-%d7/%a0-%a7,regCPX
	movem.l	regBASIC,%d0-%d7/%a0-%a7
	movem.l	%d2/%a2,-(%sp)
	bsr.s	clock			| get system time
	move.l	%d0,systime
	movem.l	(%sp)+,%d2/%a2
	rts

|-------------------------------------------------------------------
| clock: d0 = _hz_200 (clobbers d0-d2/a0-a2)
|-------------------------------------------------------------------
clock__:
	move.l	0x4ba.w,%d0
	rts

clock:
	pea	clock__(%pc)
	move.w	#38,-(%sp)		| Supexec
	trap	#14
	addq.l	#6,%sp
	rts

|-------------------------------------------------------------------
	.data
	.even
regBASIC:
	.long	0,0,0,0,0,0,0,0			| d0-d7
	.long	0,0,0,0,0,0,0,stackend		| a0-a7

regCPX:
	.long	0,0,0,0,0,0,0,0			| d0-d7
	.long	0,0,0,0,0,0,0,0			| a0-a7

| MOBLK m2 - CAUTION: x,y,w,h of m2 and the GRECT xywh are the same!
	.globl	_m2,_xywh
_m2:	.word	0
_xywh:	.word	0
	.word	0
	.word	256
	.word	176

|-------------------------------------------------------------------
	.bss
	.even
stackCPX:
	.skip	40960
stackend:
	.skip	16
systime:
	.skip	4

| ctrl and ctrlsq are tested together as one word (PutChar, cpx_key)
	.globl	_ctrl,_ctrlsq
_ctrl:	.skip	1		| flag for pressing Ctrl-C
_ctrlsq:
	.skip	1		| flag for pressing Ctrl-S/Q
