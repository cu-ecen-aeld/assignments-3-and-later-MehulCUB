# Kernel oops message
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------

#  output of echo “hello_world” > /dev/faulty
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042034000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 153 Comm: sh Tainted: G           O      5.10.7 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xf0/0x290
sp : ffffffc010ccbdc0
x29: ffffffc010ccbdc0 x28: ffffff80020c8000 
x27: 0000000000000000 x26: 0000000000000000 
x25: 0000000000000000 x24: 0000000000000000 
x23: 0000000000000000 x22: ffffffc010ccbe30 
x21: 0000005579981940 x20: ffffff800204b000 
x19: 0000000000000012 x18: 0000000000000000 
x17: 0000000000000000 x16: 0000000000000000 
x15: 0000000000000000 x14: 0000000000000000 
x13: 0000000000000000 x12: 0000000000000000 
x11: 0000000000000000 x10: 0000000000000000 
x9 : 0000000000000000 x8 : 0000000000000000 
x7 : 0000000000000000 x6 : 0000000000000000 
x5 : ffffff8002261d98 x4 : ffffffc0086c7000 
x3 : ffffffc010ccbe30 x2 : 0000000000000012 
x1 : 0000000000000000 x0 : 0000000000000000 
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 el0_svc_common.constprop.0+0x94/0x1c0
 do_el0_svc+0x40/0xb0
 el0_svc+0x14/0x20
 el0_sync_handler+0x1a4/0x1b0
 el0_sync+0x174/0x180
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace a5cddff76c172ce6 ]---
```

# Analysis of kernel oops message


-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
* Error Line: "Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000" 
 - this line indicates that there is a NULL pointer exception at address 0.

* Error Line: Internal error: Oops: 96000045 [#1] SMP
- Here [#1] — is the number of times the Oops occurred. 
 
* Error Line: pc : faulty_write+0x14/0x20 [faulty]
- this line indicates that error occurred faulty_write function call 
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------

* In faulty.c => faulty_write function

```
size_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;  //#####  NULL pointer dereference
	return 0;
}
```
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Dissembly for faulty.ko

/home/mehul/Desktop/f21_aesd/aesd7/assignment-5-MehulCUB/buildroot/output/build/ldd-896a85f2bc1471f23bf6fa22817695aca2e80fe5/misc-modules/faulty.ko:     file format elf64-littleaarch64

Disassembly of section .text:
```
0000000000000000 <faulty_write>:
   0:	d503245f 	bti	c
   4:	d2800001 	mov	x1, #0x0                   	// #0
   8:	d2800000 	mov	x0, #0x0                   	// #0
   c:	d503233f 	paciasp
  10:	d50323bf 	autiasp
  14:	b900003f 	str	wzr, [x1]
  18:	d65f03c0 	ret
  1c:	d503201f 	nop

0000000000000020 <faulty_read>:
  20:	d503233f 	paciasp
  24:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
  28:	d5384100 	mrs	x0, sp_el0
  2c:	910003fd 	mov	x29, sp
  30:	f9000bf3 	str	x19, [sp, #16]
  34:	92800005 	mov	x5, #0xffffffffffffffff    	// #-1
  38:	12800003 	mov	w3, #0xffffffff            	// #-1
  3c:	f100105f 	cmp	x2, #0x4
  40:	f9420804 	ldr	x4, [x0, #1040]
  44:	f90017e4 	str	x4, [sp, #40]
  48:	d2800004 	mov	x4, #0x0                   	// #0
  4c:	910013e0 	add	x0, sp, #0x4
  50:	92800004 	mov	x4, #0xffffffffffffffff    	// #-1
  54:	d2800093 	mov	x19, #0x4                   	// #4
  58:	9a939053 	csel	x19, x2, x19, ls  // ls = plast
  5c:	a9021404 	stp	x4, x5, [x0, #32]
  60:	b90037e3 	str	w3, [sp, #52]
  64:	d5384103 	mrs	x3, sp_el0
  68:	b9402c64 	ldr	w4, [x3, #44]
  6c:	f9400460 	ldr	x0, [x3, #8]
  70:	37a80324 	tbnz	w4, #21, d4 <faulty_read+0xb4>
  74:	f9400063 	ldr	x3, [x3]
  78:	aa0103e2 	mov	x2, x1
  7c:	7206007f 	tst	w3, #0x4000000
  80:	540002a1 	b.ne	d4 <faulty_read+0xb4>  // b.any
  84:	ab130042 	adds	x2, x2, x19
  88:	9a8083e0 	csel	x0, xzr, x0, hi  // hi = pmore
  8c:	da9f3042 	csinv	x2, x2, xzr, cc  // cc = lo, ul, last
  90:	fa00005f 	sbcs	xzr, x2, x0
  94:	9a9f87e2 	cset	x2, ls  // ls = plast
  98:	aa1303e0 	mov	x0, x19
  9c:	b50002e2 	cbnz	x2, f8 <faulty_read+0xd8>
  a0:	7100001f 	cmp	w0, #0x0
  a4:	d5384101 	mrs	x1, sp_el0
  a8:	93407c00 	sxtw	x0, w0
  ac:	9a931000 	csel	x0, x0, x19, ne  // ne = any
  b0:	f94017e2 	ldr	x2, [sp, #40]
  b4:	f9420823 	ldr	x3, [x1, #1040]
  b8:	eb030042 	subs	x2, x2, x3
  bc:	d2800003 	mov	x3, #0x0                   	// #0
  c0:	54000361 	b.ne	12c <faulty_read+0x10c>  // b.any
  c4:	f9400bf3 	ldr	x19, [sp, #16]
  c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
  cc:	d50323bf 	autiasp
  d0:	d65f03c0 	ret
  d4:	9340dc22 	sbfx	x2, x1, #0, #56
  d8:	8a020022 	and	x2, x1, x2
  dc:	ab130042 	adds	x2, x2, x19
  e0:	9a8083e0 	csel	x0, xzr, x0, hi  // hi = pmore
  e4:	da9f3042 	csinv	x2, x2, xzr, cc  // cc = lo, ul, last
  e8:	fa00005f 	sbcs	xzr, x2, x0
  ec:	9a9f87e2 	cset	x2, ls  // ls = plast
  f0:	aa1303e0 	mov	x0, x19
  f4:	b4fffd62 	cbz	x2, a0 <faulty_read+0x80>
  f8:	d503201f 	nop
  fc:	9340dc22 	sbfx	x2, x1, #0, #56
 100:	d5384100 	mrs	x0, sp_el0
 104:	8a020022 	and	x2, x1, x2
 108:	f9400403 	ldr	x3, [x0, #8]
 10c:	ea23005f 	bics	xzr, x2, x3
 110:	9a9f0020 	csel	x0, x1, xzr, eq  // eq = none
 114:	d503229f 	csdb
 118:	910093e1 	add	x1, sp, #0x24
 11c:	aa1303e2 	mov	x2, x19
 120:	94000000 	bl	0 <__arch_copy_to_user>
 124:	d503201f 	nop
 128:	17ffffde 	b	a0 <faulty_read+0x80>
 12c:	94000000 	bl	0 <__stack_chk_fail>

0000000000000130 <faulty_init>:
 130:	d503233f 	paciasp
 134:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
 138:	90000004 	adrp	x4, 0 <faulty_write>
 13c:	910003fd 	mov	x29, sp
 140:	f9000bf3 	str	x19, [sp, #16]
 144:	90000013 	adrp	x19, 0 <faulty_write>
 148:	b9400260 	ldr	w0, [x19]
 14c:	90000003 	adrp	x3, 0 <faulty_write>
 150:	91000084 	add	x4, x4, #0x0
 154:	91000063 	add	x3, x3, #0x0
 158:	52802002 	mov	w2, #0x100                 	// #256
 15c:	52800001 	mov	w1, #0x0                   	// #0
 160:	94000000 	bl	0 <__register_chrdev>
 164:	37f800a0 	tbnz	w0, #31, 178 <faulty_init+0x48>
 168:	b9400261 	ldr	w1, [x19]
 16c:	350000e1 	cbnz	w1, 188 <faulty_init+0x58>
 170:	b9000260 	str	w0, [x19]
 174:	52800000 	mov	w0, #0x0                   	// #0
 178:	f9400bf3 	ldr	x19, [sp, #16]
 17c:	a8c27bfd 	ldp	x29, x30, [sp], #32
 180:	d50323bf 	autiasp
 184:	d65f03c0 	ret
 188:	52800000 	mov	w0, #0x0                   	// #0
 18c:	f9400bf3 	ldr	x19, [sp, #16]
 190:	a8c27bfd 	ldp	x29, x30, [sp], #32
 194:	d50323bf 	autiasp
 198:	d65f03c0 	ret
 19c:	d503201f 	nop

00000000000001a0 <cleanup_module>:
 1a0:	d503233f 	paciasp
 1a4:	90000000 	adrp	x0, 0 <faulty_write>
 1a8:	a9bf7bfd 	stp	x29, x30, [sp, #-16]!
 1ac:	52802002 	mov	w2, #0x100                 	// #256
 1b0:	52800001 	mov	w1, #0x0                   	// #0
 1b4:	910003fd 	mov	x29, sp
 1b8:	b9400000 	ldr	w0, [x0]
 1bc:	90000003 	adrp	x3, 0 <faulty_write>
 1c0:	91000063 	add	x3, x3, #0x0
 1c4:	94000000 	bl	0 <__unregister_chrdev>
 1c8:	a8c17bfd 	ldp	x29, x30, [sp], #16
 1cc:	d50323bf 	autiasp
 1d0:	d65f03c0 	ret
 1d4:	d500409f 	msr	pan, #0x0
 1d8:	d500419f 	msr	pan, #0x1

Disassembly of section .plt:

0000000000000300 <.plt>:
	...

Disassembly of section .text.ftrace_trampoline:

0000000000000302 <.text.ftrace_trampoline>:
```



-----------------------------------------------------------------------------------------------------------------------------------------------------------------------

# Analysis for objdump 

in the <faulty_write> section 
here lines 
*  line 4:	d2800001 	mov	x1, #0x0   ; assigns 0x0 to x1 
*  line 14:	b900003f 	str	wzr, [x1]  ; attempts to access at content at dereference by x1  - i.e. dereferencing a NULL pointer
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------

* From the above calltrace and objdump - disassembly for faulty_write, we can see that when faulty_write is called, kernel tries accessing data referenced by NULL pointer, and it throws exception for  NULL pointer dereference for invalid memory access

* Thus, we can search "Oops" to search all oops messages and check objdump- disassembly and trace instruction that caused fault and type of the fault. 
