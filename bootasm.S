#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# TODO: qemu-dump に全て説明させて、ここのコメントは極力消す

# Start the first CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.code16                       # Assemble for 16-bit mode
                              # bootblock.asm will be broken
.globl start
start:
  # initial eax: 0xaa55
  # sign.pl: 0x55aa

#if 0 // experiment_mov
  # http://caspar.hazymoon.jp/OpenBSD/annex/intel_arc.html
  #   Intel 8086 Family Architecture
  #
  #                                               EAX
  # /^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\
  #                                                                        AX
  #                                                  /^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\
  #                                                            AH                      AL
  #                                                  /^^^^^^^^^^^^^^^^^^^^\ /^^^^^^^^^^^^^^^^^^^^^\
  # 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00

  # https://en.wikibooks.org/wiki/X86_Assembly/GAS_Syntax
  # > b = byte (8 bit).
  # > s = single (32-bit floating point).
  # > w = word (16 bit).
  # > l = long (32 bit integer or 64-bit floating point).
  # > q = quad (64 bit).
  # > t = ten bytes (80-bit floating point).

  movl $0xffffffff, %eax  # ffffffff  66 b8 ff ff ff ff
  movb $0x11,       %al   # ffffff11  b0 11
  movb $0x22,       %ah   # ffff2211  b4 22
  movw $0x3333,     %ax   # ffff3333  b8 33 33
  movl $0x44444444, %eax  # 44444444  66 b8 44 44 44 44
  # Error: unsupported instruction `movq'
  # Error: bad register name `%rax'
  # movq $0x5555555555555555, %rax

  # same

  mov  $0xffffffff, %eax  # ffffffff  66 b8 ff ff ff ff
  mov  $0x11,       %al   # ffffff11  b0 11
  mov  $0x22,       %ah   # ffff2211  b4 22
  mov  $0x3333,     %ax   # ffff3333  b8 33 33
  mov  $0x44444444, %eax  # 44444444  66 b8 44 44 44 44

  movl $0xffffffff, %eax  # ffffffff
  mov  $0x55,       %al   # ffffff55  b0 55
  mov  $0x66,       %ah   # ffff66ff  b4 66
  mov  $0x77,       %ax   # ffff0077  b8 77 00
  mov  $0x88,       %eax  # 00000088  66 b8 88 00 00 00

  # http://caspar.hazymoon.jp/OpenBSD/annex/gate_a20.html gate A20

  # 1: movw   $0xffff,%ds    /* データセグメントに0xFFFFを代入 */   # Error: invalid instruction suffix for `mov'
  # 2: movw   $0x000f, %bx   /* bx registerに0x000fを代入 */
  # 3: move   $0x1111,(%bx)  /* [0xFFFF]:[0x000F] = 0x1111 */       # Error: no such instruction: `move $0x1111,(%bx)'
  # 4: incw   $bx            /* bxをインクリメント  bx = 0x0010 */  # Error: operand type mismatch for `inc'
  # 5: move   $0x2222,(%bx)  /* [0xFFFF]:[0x0010] = 0x2222 */       # Error: no such instruction: `move $0x2222,(%bx)'
  #                           /* 80286の場合は 0x100000 = 0x2222 */
  #                           /* 8086の場合は  0x00000 = 0x2222 */
  # 6: movw   $0x0000,%ds    /* データセグメントに0x0000を代入 */   #  Error: invalid instruction suffix for `mov'
  # 7: movw   $0x0000,%bx    /* bx registerに0x0000を代入 */
  # 8: move   (%bx),$ax      /* ax = [0x0000]:[0x0000]  */          # Error: no such instruction: `move (%bx),$ax'

  # 1
  movw   $0xffff, %ax
  movw   %ax,     %ds
  # 2
  movw   $0x000f, %bx
  # 3
  movw   $0x1111, %cx
  # Error: operand type mismatch for `movs'
  # movsw  %cx,     %bx # ???
  # TODO
  movsbw %cl,     %bx # XXX
  # 4
  incw   %bx
  # 5
  movw   $0x2222, %cx
  movsbw %cl,     %bx # XXX
  # 6
  movw   $0x0000, %cx
  movw   %cx,     %ds
  # 7
  movw   $0x0000, %bx
  # 8
  # ???
  movsbw %bl,     %ax # XXX

#endif // experiment_mov

  cli                         # BIOS enabled interrupts; disable

  # Zero data segment registers DS, ES, and SS.
  # eflags: -> 0x46 [ PF(0x04) ZF(0x40) ]; ax has became even and zero
  xorw    %ax,%ax             # Set %ax to zero
  movw    %ax,%ds             # -> Data Segment
  movw    %ax,%es             # -> Extra Segment
  movw    %ax,%ss             # -> Stack Segment

#ifndef __clang__ // for MBR <= 512 bytes
  # Physical address line A20 is tied to zero so that the first PCs 
  # with 2 MB would run software that assumed 1 MB.  Undo that.
seta20.1:
  # port 0x64 kbd status: 28 0x1c, even -> not busy
  # test: http://softwaretechnique.jp/OS_Development/Tips/IA32_Instructions/TEST.html
  # 0x2 & al -> 0; SF=0 ZF=1 PF=1 CF=0 OF=0
  # jnz not taken since ZF=1
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1
  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60
#endif // __clang__

  # add-symbol-file で全部入れてると x gdtdesc のアドレスがおかしい…
  # やはり無理があるか
  # phaseごとに分けて読み込まないと無理かな

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  #
  # lgdt: needed for ljmp
  # QEMU:
  # tcg_gen_st_tl(s->T0, cpu_env, offsetof(CPUX86State, gdt.base));
  # tcg_gen_st32_tl(s->T1, cpu_env, offsetof(CPUX86State, gdt.limit));
  # gdt.limit: 0->0x17 (uint32_t)
  # gdt.base: 0->0x7c60 (target_ulong, uint32_t; actually 16bits here)
  #
  # rw -l env->gdt: ljump -> load_segment_ra()
  lgdt    gdtdesc
  #
  # (gdb) aw -l env->cr
  # (gdb) aw -l env->segs
  # Could not insert hardware watchpoint 7.
  # Could not insert hardware breakpoints:
  # You may have requested too many hardware breakpoints/watchpoints.
  #
  # (gdb) aw -l env->cr[0]
  # (gdb) aw -l env->segs[R_ES]
  # (gdb) aw -l env->segs[R_SS]
  # (gdb) aw -l env->segs[R_DS]
  # (gdb) aw -l env->segs[R_FS]
  # (gdb) aw -l env->segs[R_GS]
  # still too many...
  #
  # cpu_x86_load_seg_cache() R_DS R_ES R_SS -> hflags
  # so complex...
  #
  # regs[R_EAX]: 0xdf->0x10
  #   TODO: 0x10: 0x8 0x2 は何だ？ qemuに説明させる
  # code_gen_buffer() helper_read_crN()
  # 0x10 16 0b00010000
  #              CR0_ET_MASK
  movl    %cr0, %eax
  # regs[R_EAX]: 0x10->0x11
  # eflags: 0x46->0x6 -CC_Z(1<<6 0x40)
  orl     $CR0_PE, %eax
  # cr[0]: 0x10->0x11
  # code_gen_buffer() helper_write_crN() cpu_x86_update_cr0()
  movl    %eax, %cr0

//PAGEBREAK!
  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  #
  # book:
  # > One cannot directly modify %cs,
  # > so instead the code executes an ljmp (far jump) instruction (9153), which allows a code
  # > segment selector to be specified.
  #
  # $cs 0 -> 8 (SEG_KCODE<<3 == 1<<3)
  # QEMU:
  # code_gen_buffer
  #   helper_ljmp_protected() new_cs:8 new_eip:0x7c31(start32)
  #     load_segment_ra()
  # segs[R_CS].selector: 0->8
  # segs[R_CS].limit: 0xffff->0xffffffff
  # segs[R_CS].flags: 0x9b0f->0xcf9a00
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now.
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
                                  # # regs[R_EAX]: 0x11->0x10
  movw    %ax, %ds                # -> DS: Data Segment
                                  # segs[R_DS].selector: 0->16
                                  # segs[R_DS].limit: 0xffff->0xffffffff
  movw    %ax, %es                # -> ES: Extra Segment
                                  # segs[R_ES].selector: 0->16
                                  # segs[R_ES].limit: 0xffff->0xffffffff
  movw    %ax, %ss                # -> SS: Stack Segment
                                  # segs[R_SS].selector: 0->16
                                  # segs[R_SS].limit: 0xffff->0xffffffff
  movw    $0, %ax                 # Zero segments not ready for use
                                  # regs[R_EAX]: 0x10->0
  movw    %ax, %fs                # -> FS
                                  # segs[R_FS].limit: 0xffff->0
  movw    %ax, %gs                # -> GS
                                  # segs[R_GS].limit: 0xffff->0

  # Set up the stack pointer and call into C.
  # regs[R_ESP]: 0x6ef0->0x7c00
  movl    $start, %esp

  # x/8xb start-8
  # 0x7bf8:	0x00	0x00	0x00	0x00	0x00	0x00	0x00	0x00
  # ↓
  # 0x7bf8:	0x00	0x00	0x00	0x00	0x4d	0x7c	0x00	0x00
  #
  # returns 0x00007c4d
  #
  # call    bootmain
  #   7c48:	e8 f0 00 00 00       	call   7d3d <bootmain>
  # movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  #   7c4d:	66 b8 00 8a          	mov    $0x8a00,%ax
  #
  # regs[R_ESP]: 0x7c00->0x7bfc
  call    bootmain

  # If bootmain returns (it shouldn't), trigger a Bochs
  # '
  # breakpoint if running under Bochs, then loop.
  movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
  outw    %ax, %dx
spin:
  jmp     spin

# 0x7c60  gdt      24 bytes (8 bytes x3 (null seg, SEG_KCODE, SEG_KDATA))
# 0x7c78  gdtdesc   6 bytes
# 0x7c7e <waitdisk>
#
# (gdb) x/33xb gdt
# 0x7c60 <gdt>:        0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00  SEG_NULLASM(0)  0<<3 = 0
# 0x7c68 <gdt+8>:      0xff 0xff 0x00 0x00 0x00 0x9a 0xcf 0x00  SEG_KCODE(1)    1<<3 = 0b00001000 (8)   structure is very complex... (see asm.h SEG_ASM)
# 0x7c70 <gdt+16>:     0xff 0xff 0x00 0x00 0x00 0x92 0xcf 0x00  SEG_KDATA(2)    2<<3 = 0b00010000 (16)
# 0x7c78 <gdtdesc>:    0x17 0x00 0x60 0x7c 0x00 0x00 0xf3 0x0f
#                        ^^   ^^                                size: 0x0017 = 23 (sizeof(gdt) - 1 = 24 - 1)
#                                  ^^   ^^   ^^   ^^            0x00007c60
#                                                                 (gdb) info symbol 0x7c60
#                                                                 gdt in section .text
#                                                      ^^   ^^  <waitdisk>
# 0x7c80 <waitdisk+2>: 0x1e

# TODO: qemu-dump: STA_X STA_R STA_W もダンプ
#   currently:
#   0x00007c1d: 0f 01 16 7c78
#   0x00007c1d:  0f 01 16 78 7c           lgdtw    0x7c78
#   gdt.base: 0->0x7c60
#   gdt.limit: 0->0x17
#   gdt updated:
#     0 00007c60: 0x0000000000000000 (base:0 limit:0)
#     1 00007c68: 0xffff0000009acf00 (base:0 limit:0xffffffff)
#     2 00007c70: 0xffff00000092cf00 (base:0 limit:0xffffffff)

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  # #define SEG_ASM(type,base,lim)
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt                             # address gdt
