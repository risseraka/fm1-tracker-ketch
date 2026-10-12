/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* App entry, at XIP 0x02001000 behind the boot stub (firmware/stub): the stub
 * comes here once the body's commit marker checks out. The SPL's entry
 * convention (`goto +4; rts` first) is kept. The core state the SPL left is
 * saved to STATUS by the stub's crt0, not here (it would be the stub's state). */
	.section .text.crt0,"ax",@progbits
	.globl _start
_start:
	goto 1f
	rts
1:	cli
	sp = _ustack_top
	ssp = _sstack_top
	call fm1_cstart
	goto .
