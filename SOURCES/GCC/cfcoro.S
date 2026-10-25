|
| cfcoro.S - the BASIC coroutine for ColdFire (FireBee build)
|
| ColdFire version of the context switch in cpxmains.S. ColdFire has no
| movem with -(sp)/(sp)+ or absolute addresses, so the registers are
| stored through a0. Every switch happens inside a C call, so only the
| registers a C function has to keep (d2-d7/a2-a6) and the stack pointer
| are saved. The rest of cpxmains.S (BreakBASIC, ContBASIC, WaitBASIC,
| basic_exit) is C in cfglue.c.
|
| context buffer: d2-d7/a2-a6 (44 bytes), then sp
|

	.text

|-------------------------------------------------------------------
| long cf_to_basic(void)  -  from the GEM side into BASIC; returns
| the value BASIC passes to cf_to_gem()
|-------------------------------------------------------------------
	.globl	_cf_to_basic
_cf_to_basic:
	lea	regGEM,%a0
	movem.l	%d2-%d7/%a2-%a6,(%a0)
	move.l	%sp,44(%a0)
	lea	regBASIC,%a0
	movem.l	(%a0),%d2-%d7/%a2-%a6
	move.l	44(%a0),%sp
	moveq	#0,%d0			| cf_to_gem() returns 0 to BASIC
	rts

|-------------------------------------------------------------------
| long cf_to_gem(long value)  -  from BASIC back to the GEM side, where
| cf_to_basic() returns `value'
|-------------------------------------------------------------------
	.globl	_cf_to_gem
_cf_to_gem:
	move.l	4(%sp),%d0
	lea	regBASIC,%a0
	movem.l	%d2-%d7/%a2-%a6,(%a0)
	move.l	%sp,44(%a0)
	lea	regGEM,%a0
	movem.l	(%a0),%d2-%d7/%a2-%a6
	move.l	44(%a0),%sp
	rts

|-------------------------------------------------------------------
| void StartBASIC(void)  -  starts basic() on its own stack; it comes
| back here when BASIC waits (cf_to_gem)
|-------------------------------------------------------------------
	.globl	_StartBASIC
_StartBASIC:
	lea	regGEM,%a0
	movem.l	%d2-%d7/%a2-%a6,(%a0)
	move.l	%sp,44(%a0)
	lea	stackend,%sp
	moveq	#-1,%d0
	move.b	%d0,_ctrlsq
	move.l	#_basic_exit,-(%sp)	| basic() returns into basic_exit
	jmp	_basic

|-------------------------------------------------------------------
	.bss
	.balign	4
regBASIC:
	.skip	48
regGEM:
	.skip	48
stack:
	.skip	40960
stackend:
	.skip	16

| ctrl and ctrlsq are tested together as one word (PutChar, Ctrl-C)
	.balign	2
	.globl	_ctrl,_ctrlsq
_ctrl:	.skip	1		| flag for pressing Ctrl-C
_ctrlsq:
	.skip	1		| flag for pressing Ctrl-S
