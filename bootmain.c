// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

#ifdef __clang__
// __attribute__((optnone))
#else
// boot block too large: 559 bytes (max 510)
// __attribute__((optimize("O0")))
// __attribute__((optimize("Og")))
#endif
void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space

  // TODO: 2020-08-30:
  //  - readseg() でのメモリの変化の様子可視化
  //    - CLion memory view or peda?
  //    - できればelfのAA描く
  //  - step into entry()

  // Read 1st page off disk
  readseg((uchar*)elf, 4096, 0);

  // Is this an ELF executable?
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  // end proghdr
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    // TODO: ↓ ? memsz と filesz がなんのことか分からん
    // qemu-dump 長過ぎる… と思ったら c の場合はそうでもないな; tcg-block が rep を避けたからか
    if(ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void(*)(void))(elf->entry);
  // entry: 0x1000c
  // (gdb) disas
  // => 0x00007dc0 <+119>:	call   *0x10018
  // (gdb) p *0x10018
  // 0x10000c
  // (gdb) si
  // -> 0x00010000c in ?? ()
  //
  // -> entry.S entry
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  // 0xc0: 0b11000000
  // 0x40: 0b01000000
  // http://bochs.sourceforge.net/techspec/PORTS.LST
  // 01F0-01F7 ----	HDC 1	(1st Fixed Disk Controller)
  // 01F7	r	status register
  // 		 bit 7 = 1  controller is executing a command
  // 		 bit 6 = 1  drive is ready
  // waiting (not executing a command) && (ready)
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// won't boot -- elf->magic != ELF_MAGIC
// #ifdef __clang__
// // for MBR size 512
// // #define waitdisk() asm("nop") // boot block is 508 bytes (max 510)
// #define waitdisk() do {} while (0) // boot block is 506 bytes (max 510)
// #endif

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  // 01F2	r/w	sector count
  // 01F3	r/w	sector number
  // 01F4	r/w	cylinder low
  // 01F5	r/w	cylinder high
  // 01F6	r/w	drive/head
  //   0xe0: 0b11100000
  //            ^ why not 0? (why not 0xa0?)
  // 		 bit 7	 = 1
  // 		 bit 6	 = 0
  // 		 bit 5	 = 1
  // 		 bit 4	 = 0  drive 0 select
  // 			 = 1  drive 1 select
  // 		 bit 3-0      head select bits
  // 01F7	w	command register
  //   0x20: 0b00100000
  // 		 20	 read sectors with retry
  // see md
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  // SECTSIZE/4 == 128 == 0x80; mov $0x80,%ecx
  insl(0x1F0, dst, SECTSIZE/4);
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
void
readseg(uchar* pa, uint count, uint offset)
{
  // end physical address
  uchar* epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1;

  // pa: 0x10000
  // offset: 1
  // readsect()
  // (gdb) info reg ecx edx es edi
  // (gdb) si
  // (gdb) si 100
  // cld
  //   ecx 128
  //   edx 0x1f0
  //   es  0x10
  //   edi 0x10000
  // rep insl (%dx),%es:(%edi)
  //   ecx -> 127     -> 126     -> ... -> 2       -> 1       -> 0
  //   edi -> 0x10004 -> 0x10008 -> ... -> 0x101f8 -> 0x101fc -> 0x10200
  //                                                          ^ next inst (pop)

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
