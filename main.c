#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
// kernel.ld: PROVIDE(end = .);
// watch on clion:
// - (void *)end
// - (void *)PGROUNDUP((uint)end)
extern char end[]; // first address after kernel loaded from ELF file

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  // initialize:
  // (void *)PGROUNDUP((uint)end) <= (this range) < 8040_0000 (KERNBASE + 4Mi)
  //
  // PGSIZE 4096 0x1000 4Ki
  //
  // ooo: not include
  // xxx:     include
  //
  // P2V(4Mi)     0x8040_0000 ooo
  //              0x803f_f000  |  <- kmem.freelist right after kinit1()
  //              0x803f_e000  |
  //              0x803f_d000  |
  //              0x803f_c000  |
  //                           |
  // ROUNDUP      0x8011_6000 xxx
  // (void *)end  0x8011_5488
  // KERNBASE     0x8000_0000
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  {
    // evaluate: (struct run *)0x80117000
    // kmem.freelist -> 803ff000 -> 803fe000 -> 803fd000 -> ... ->
    //   0x80117000 -> 0x80116000 -> NULL

    // kfree((char *)0x803fd000); // collapse

    // kmem.freelist -> 803ff000 -> 803fe000 -> 803fd000 -> ...
    void *tmp1 = kalloc(); // 803ff000
    // kmem.freelist ->             803fe000 -> 803fd000 -> ...
    void *tmp2 = kalloc(); // 803fe000
    // kmem.freelist ->                         803fd000 -> ...
    kfree(tmp1);
    // kmem.freelist -> 803ff000 ->             803fd000 -> ...
    void *tmp3 = kalloc(); // 803ff000
    // kmem.freelist ->                         803fd000 -> ...
    kfree(tmp2);
    // kmem.freelist ->             803fe000 -> 803fd000 -> ...
    kfree(tmp3);
    // kmem.freelist -> 803ff000 -> 803fe000 -> 803fd000 -> ...
  }
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  log_info("console_init() done"); // printed out to only vga
  uartinit();      // serial port
  log_info("uart_init() done"); // printed out to both vga and console
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  // TODO
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  // (gdb) info r eip
  // eip            0x80103b34          0x80103b34 <mpenter>
  // switchkvm() (cr3設定の前) からvirtual address?? lgdtで設定済だったっけ？
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  // cpu0,1 で同じIDTを使う
  // IO/APICからの割り込みは ioapicenable(IRQ_*, <cpu>) で指定したcpuに飛ぶ
  idtinit();       // load idt register
  log_debug("mycpu()->started = 1");
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  // // IRQ_COM1 がcpu1で起きない実験
  // if (cpuid() == 0) {
  //   for (;;)
  //       asm("nop");
  // }
  scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  log_debug("");
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    log_debug("wait c->started");

    // wait for cpu to finish mpmain()
    while(c->started == 0)
#if 0
      ;
#else /* 0 */
      asm("nop");
#endif /* 0 */
    log_debug("accept c->started == %d", c->started);
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

// &entrypgdir 0x8010a000
// entrypgdir[0]   = 163 0xa3 0b10100011
// entrypgdir[512] = 227 0xe3 0b11100011
//                              ^PS   ^^W P
//                               ^^???
// VA [0, 4MB) == VA [KERNBASE, KERNBASE+4MB]
// (gdb) x/32bx 0x00000000
// 0x0:	0x53	0xff	0x00	0xf0	0x53	0xff	0x00	0xf0
// 0x8:	0xc3	0xe2	0x00	0xf0	0x53	0xff	0x00	0xf0
// 0x10:	0x53	0xff	0x00	0xf0	0x54	0xff	0x00	0xf0
// 0x18:	0x53	0xff	0x00	0xf0	0x53	0xff	0x00	0xf0
// (gdb) x/32bx 0x80000000
// 0x80000000:	0x53	0xff	0x00	0xf0	0x53	0xff	0x00	0xf0
// 0x80000008:	0xc3	0xe2	0x00	0xf0	0x53	0xff	0x00	0xf0
// 0x80000010:	0x53	0xff	0x00	0xf0	0x54	0xff	0x00	0xf0
// 0x80000018:	0x53	0xff	0x00	0xf0	0x53	0xff	0x00	0xf0

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  // p.37 the x86's 4-megabytes "super pages" ... PTE_PS ...
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

