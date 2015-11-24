	.section .text

    # reserve space for pointers to argument 1 and 2
_p_arg0:
    .skip 4, 0
_p_arg1:
    .skip 4, 0

	.proc
_start::
	# Set the start address of the stack 
	c0	mov $r0.1 = __STACK_START
;;
	# Write 0's to _BSS
	c0	mov $r0.21 = __BSS_START
;;
	c0	mov $r0.22 = __BSS_END
;;
	c0	cmpleu $b0.0 = $r0.22, $r0.21
;;
	c0	br $b0.0, 2f
;;
1:
	c0	stw 0x0[$r0.21] = $r0.0
	c0	cmpge $b0.0 = $r0.21, $r0.22
;;
	c0	add $r0.21 = $r0.21, 0x4
	c0	brf $b0.0, 1b
;;
2:
    c0  ldw $r0.3 = _p_arg0[$r0.0]
;;
    c0  ldw $r0.4 = _p_arg1[$r0.0]
;;
	c0	call $l0.0 = _dot_product_workgroup_fast
;;
__END:
	c0	stop
;;
	c0	nop
;;
	c0	nop
;;
.endp
