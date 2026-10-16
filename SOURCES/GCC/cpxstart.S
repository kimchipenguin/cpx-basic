|
| cpxstart.S - CPX entry and XControl glue for the GCC build
|
| XControl jumps to the start of the text segment with the XCPB pointer
| on the stack (cdecl). The MiNT a.out header at the start of the text
| segment begins with a jump to the entry point, so __start is reached
| from there.
|
| XControl passes and expects 16-bit WORDs on the stack, but GCC is
| used with 32-bit int here (MiNTLib has no -mshort libc). The thunks
| below repack the arguments for the callbacks that take WORDs, and
| xcall() calls XControl's own functions with a WORD argument list.
|
	.text
	.globl	__start

__start:
	move.l	4(%sp),-(%sp)		| XCPB *
	jsr	_cpx_init
	addq.l	#4,%sp
	move.l	%d0,%a0			| CPX_INFO *, in d0 and a0
	rts

|------------------------------------------------------------------
| VOID cpx_key(WORD kstate, WORD key, WORD *event)
|   in:  4(sp) kstate.w, 6(sp) key.w, 8(sp) event.l
|------------------------------------------------------------------
	.globl	_cpx_key_t
_cpx_key_t:
	move.l	8(%sp),-(%sp)		| event
	move.w	6+4(%sp),%d0		| key
	ext.l	%d0
	move.l	%d0,-(%sp)
	move.w	4+8(%sp),%d0		| kstate
	ext.l	%d0
	move.l	%d0,-(%sp)
	jsr	_cpx_key
	lea	12(%sp),%sp
	rts

|------------------------------------------------------------------
| VOID cpx_button(MRETS *mrets, WORD nclicks, WORD *event)
|   in:  4(sp) mrets.l, 8(sp) nclicks.w, 10(sp) event.l
|------------------------------------------------------------------
	.globl	_cpx_button_t
_cpx_button_t:
	move.l	10(%sp),-(%sp)		| event
	move.w	8+4(%sp),%d0		| nclicks
	ext.l	%d0
	move.l	%d0,-(%sp)
	move.l	4+8(%sp),-(%sp)		| mrets
	jsr	_cpx_button
	lea	12(%sp),%sp
	rts

|------------------------------------------------------------------
| VOID cpx_close(WORD app_term)
|------------------------------------------------------------------
	.globl	_cpx_close_t
_cpx_close_t:
	move.w	4(%sp),%d0
	ext.l	%d0
	move.l	%d0,-(%sp)
	jsr	_cpx_close
	addq.l	#4,%sp
	rts

|------------------------------------------------------------------
| long xcall(void *func, const WORD *args, long nwords)
|
| Calls an XControl function with nwords WORDs as its (cdecl) stack
| arguments. Saves all registers GCC expects to survive, XControl
| itself may not preserve d2/a2.
|------------------------------------------------------------------
	.globl	_xcall
_xcall:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)	| 11 registers = 44 bytes
	move.l	48(%sp),%a3		| func
	move.l	52(%sp),%a4		| args
	move.l	56(%sp),%d3		| nwords
	move.l	%sp,%a5
	move.l	%d3,%d4
	add.l	%d4,%d4
	add.l	%d4,%a4			| behind the last word
	bra.s	2f
1:	move.w	-(%a4),-(%sp)
2:	dbra	%d3,1b
	jsr	(%a3)
	move.l	%a5,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
