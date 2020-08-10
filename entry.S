# The xv6 kernel starts executing in this file. This file is linked with
# the kernel C code, so it can refer to kernel symbols such as main().
# The boot block (bootasm.S and bootmain.c) jumps to entry below.
        
# Multiboot header, for multiboot boot loaders like GNU Grub.
# http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
#
# Using GRUB 2, you can boot xv6 from a file stored in a
# Linux file system by copying kernel or kernelmemfs to /boot
# and then adding this menu entry:
#
# menuentry "xv6" {
# 	insmod ext2
# 	set root='(hd0,msdos1)'
# 	set kernel='/boot/kernel'
# 	echo "Loading ${kernel}..."
# 	multiboot ${kernel} ${kernel}
# 	boot
# }

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# Multiboot header.  Data to direct multiboot loader.
.p2align 2
.text
.globl multiboot_header
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# By convention, the _start symbol specifies the ELF entry point.
# Since we haven't set up virtual memory yet, our entry point is
# the physical address of 'entry'.
# '
.globl _start
_start = V2P_WO(entry)

# Entering xv6 on boot processor, with paging off.
.globl entry
entry:
  # breakpoint
  nop
  # Turn on page size extension for 4Mbyte pages
  movl    %cr4, %eax
  # eflags 0x46 [ IOPL=0 ZF PF ]
  # cr4    0x0  [ ]
  orl     $(CR4_PSE), %eax
  # eflags 0x2  [ IOPL=0 ]
  movl    %eax, %cr4
  # cr4    0x10 [ PSE ]

  # Set page directory
  # cr3 0x0      [ PDBR=0 PCID=0 ]
  movl    $(V2P_WO(entrypgdir)), %eax
  movl    %eax, %cr3
  # cr3 0x10a000 [ PDBR=0 PCID=0 ]

  # Turn on paging.
  # cr0 0x11       [ ET PE ]
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0
  # cr0 0x80010011 [ PG WP ET PE ]

  # Set up the stack pointer.
  # esp 0x7bec
  movl $(stack + KSTACKSIZE), %esp
  # esp 0x8010c5f0
  # gdb99 -q kernel  -ex 'info address stack' -ex 'q'  # 0x8010b5f0

  # Jump to main(), and switch to executing at
  # high addresses. The indirect call is needed because
  # the assembler produces a PC-relative instruction
  # for a direct jump.
  mov $main, %eax
  # eax 0x80103a63
  # gdb99 -q kernel  -ex 'info address main' -ex 'q'  # 0x80103a63
  # add-symbol-file kernel
  # si
  jmp *%eax

.comm stack, KSTACKSIZE
