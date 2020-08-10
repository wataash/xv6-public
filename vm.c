#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
#ifndef __clang__
// __attribute__((optimize("O0"))) // won't boot
__attribute__((optimize("Og")))
#endif
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.

  // readeflags():
  //   eflags: 0x86 [ IOPL=0 SF PF ]
  //     FL_IF (0x200) not set
  c = &cpus[cpuid()];

  // TODO: PTE_W PTE_U は STA_W DPL_USER と役割被ってないの？
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
#ifndef __clang__
// __attribute__((optimize("O0"))) // won't boot
// __attribute__((optimize("Og")))
#endif
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  if ((uint)va == KERNBASE) { // kmap[0].virt 0x8000_0000 PDX:512 PTX:0
    (void)sizeof(pde_t);
    (void)PDX(va);         // 512
    (void)PDX(0xffffffff); // 1023
    (void)kalloc;          // allocated char pgdir[PGSIZE], pde_t pgdir [1024]
    (void)pgdir;           // 0x803f_f000
    (void)&pgdir[0];       // 0x803f_f000 -> 0
    (void)&pgdir[1];       // 0x803f_f004 -> 0
    (void)&pgdir[PDX(va)]; // 0x803f_f800 -> 0  pde
    (void)&pgdir[1023];    // 0x803f_fffc -> 0
    (void)&pgdir[1024];    // 0x8040_0000 (invalid)

    // pgtab = kalloc() -> 0x803f_e000 pte_t [1024] zero'ed
    // pgdir[PDX(va)] = V2P(pgtab) | PTE_P | PTE_W | PTE_U == 0x3f_e007
    // returns (pte_t *) &pgtab[PTX(va)] == &pgtab[0] == 0x803f_e000

    breakpoint();
  } else if ((uint)va == KERNLINK) { // kmap[1].virt
    breakpoint();
  } else if ((uint)va == (uint)data) { // kmap[2].virt
    breakpoint();
  } else if ((uint)va == DEVSPACE) { // kmap[3].virt
    breakpoint();
  } else {
    breakpoint();
  }

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
#if 0
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
#else /* 0 */
    if (alloc == 0) {
      return 0;
    } else {
      if ((pgtab = (pte_t *)kalloc()) == 0) {
        notreached();
        return 0;
      }
    }
#endif /* 0 */
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    (void)(void *) V2P(pgtab);                  // 0x3f_e000
    (void)(V2P(pgtab) | PTE_P | PTE_W | PTE_U); // 0x3f_e007
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
#if 0
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
#else /* 0 */
// __attribute__((optimize("O0"))) // won't boot
static int
mappages(pde_t *pgdir, const void *const va, uint size, uint pa, int perm)
#endif /* 0 */
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);

  pte = 0;
  if (a != va)
    notreached(); // `a` always aligned
  // if (a == kmap[0].virt)
  if ((uint)a == KERNBASE) {                 // kmap[0].virt
    breakpoint1((void *)last); // Og: optimized out   0x800f_f000
    (void)((0x80000000 + 0x100000 - 1) == 0x800fffff);
    (void)(PGROUNDDOWN(0x800fffff) == 0x800ff000);

    (void)(PDX(va) == 512);
    // finally:
    (void)(pgdir[PDX(va)] == (0x3fe000 | PTE_P | PTE_W | PTE_U));
    (void)((0x3fe000 | PTE_P | PTE_W | PTE_U) == 0x3fe007);
    (void)((uint)P2V(0x3fe000) == 0x803fe000); // allocated pte
    (void)0x803fe000;                          // pte_t [1024]
    (void)(((pte_t *)0x803fe000)[0] == 0x0003); // PTE_P | PTE_W(kmap[0].perm)
    (void)(((pte_t *)0x803fe000)[1] == 0x1003);
    (void)(((pte_t *)0x803fe000)[15] == 0xf003);
    (void)(((pte_t *)0x803fe000)[16] == 0x010003);
    (void)(((pte_t *)0x803fe000)[255] == 0xff0003);
    (void)(((pte_t *)0x803fe000)[256] == 0);  // NULL
    (void)(((pte_t *)0x803fe000)[1023] == 0); // NULL
    (void)((pte_t *)0x803fe000 + 1024 == (pte_t *)0x803ff000); // next page

    breakpoint();
  } else if ((uint)a == KERNLINK) { // kmap[1].virt
    // 0x803ff000
    // finally:
    (void)(pte == (pte_t *)0x803fe400);  // allocated
    breakpoint();
  } else if ((uint)a == (uint)data) { // kmap[2].virt
    breakpoint();
  } else if ((uint)a == DEVSPACE) { // kmap[3].virt
    breakpoint();
  } else {
    breakpoint();
  }

  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
#if 0
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
#else /* 0 */
static const struct kmap {
  const void *const virt;
  const uint phys_start;
  const uint phys_end;
  const int perm;
#endif /* 0 */
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  // 4096 byte; 4096/sizeof(pde_t): 1024; pgdir[1024]
  // pgdir[PDX(va)]
  pde_t *pgdir;
#if 0
  struct kmap *k;
#else /* 0 */
  const struct kmap *k;
#endif /* 0 */

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      notreached();
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  // (gdb) x/1xb 0
  // 0x0:	0x53
  lcr3(V2P(kpgdir));   // switch to the kernel page table
  // (gdb) x/1xb 0
  // 0x0:	Cannot access memory at address 0x0
}

// TODO
// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  log_info("");
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  log_info("");
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

