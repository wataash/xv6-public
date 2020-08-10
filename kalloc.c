// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  // needed?
  // if needed, why not kmem->freelist = NULL ?
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
  breakpoint();
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    // main() -> kinit1() -> kfreerange() -> won't print anything
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/**
 * @see main
 */
char*
kalloc(void)
{
  struct run *r;

  volatile int debug = 0;
  if (debug != 0) {
    volatile int breakpoint = 0;
    if ((uint)kmem.freelist != 0x803ff000)
      breakpoint |= 0x1;
    if ((uint)kmem.freelist->next != 0x803fe000)
      breakpoint |= 0x2;
    volatile struct run *tmpr;
    tmpr = kmem.freelist; // 0x803ff000
    while (1) {
      tmpr = tmpr->next;  // 0x803fe000 0x803fd000 ... 0x80117000 0x80116000 0
      if ((uint)tmpr == 0x80117000)
        breakpoint |= 0x04;
      if ((uint)tmpr == 0x80116000)
        breakpoint |= 0x08;
      if (tmpr == 0) {
        breakpoint |= 0x10;
        break;
      }
    }
    breakpoint |= 0x100;
    (void)breakpoint;
  }
  volatile struct run *kmem_freelist_prev = kmem.freelist;
  (void)kmem_freelist_prev;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

