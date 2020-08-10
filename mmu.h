// This file contains definitions for the
// x86 memory management unit (MMU).

// Eflags register
#define FL_IF           0x00000200      // Interrupt Enable

// Control Register flags
#define CR0_PE          0x00000001      // Protection Enable
#define CR0_WP          0x00010000      // Write Protect
#define CR0_PG          0x80000000      // Paging

#define CR4_PSE         0x00000010      // Page size extension

// various segment selectors.
#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_UCODE 3  // user code
#define SEG_UDATA 4  // user data+stack
#define SEG_TSS   5  // this process's task state

// cpu->gdt[NSEGS] holds the above segments.
#define NSEGS     6

#ifndef __ASSEMBLER__
// Segment Descriptor
struct segdesc {
  uint lim_15_0 : 16;  // Low bits of segment limit
  uint base_15_0 : 16; // Low bits of segment base address
  uint base_23_16 : 8; // Middle bits of segment base address
  uint type : 4;       // Segment type (see STS_ constants)
  uint s : 1;          // 0 = system, 1 = application
  uint dpl : 2;        // Descriptor Privilege Level
  uint p : 1;          // Present
  uint lim_19_16 : 4;  // High bits of segment limit
  uint avl : 1;        // Unused (available for software use)
  uint rsv1 : 1;       // Reserved
  uint db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
  uint g : 1;          // Granularity: limit scaled by 4K when set
  uint base_31_24 : 8; // High bits of segment base address
};

// Normal segment
#define SEG(type, base, lim, dpl) (struct segdesc)    \
{ ((lim) >> 12) & 0xffff, (uint)(base) & 0xffff,      \
  ((uint)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint)(lim) >> 28, 0, 0, 1, 1, (uint)(base) >> 24 }
#define SEG16(type, base, lim, dpl) (struct segdesc)  \
{ (lim) & 0xffff, (uint)(base) & 0xffff,              \
  ((uint)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint)(lim) >> 16, 0, 0, 1, 0, (uint)(base) >> 24 }
#endif

#define DPL_USER    0x3     // User DPL

// Application segment type bits
#define STA_X       0x8     // Executable segment
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)

// System segment type bits
#define STS_T32A    0x9     // Available 32-bit TSS
#define STS_IG32    0xE     // 32-bit Interrupt Gate
#define STS_TG32    0xF     // 32-bit Trap Gate

// A virtual address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(va) --/ \--- PTX(va) --/

//   0b1000...000 = 2^(number of zeroes)
//            0b1 = 2^0 = 1
//           0b10 = 2^1 = 2
//          0b100 = 2^2 = 4
//         0b1000 = 2^3 = 8
// 0b10_0000_0000 = 2^9 = 512
// 0b11_1111_1111       = 1023 (2^10-1)

// PDX: 0-1023
// PTX: 0-1023
// offset: 0-4095

// PDX  PTX  off  hex        bin
//                DD?T TOOO  DDDD DDDD DDTT TTTT TTTT OOOO OOOO OOOO
//   0    0    0  0000_0000  0000 0000 0000 0000 0000 0000 0000 0000
//   0    1    0  0000_1000  0000 0000 0000 0000 0001 0000 0000 0000
//   1    0    0  0040_0000  0000 0000 0100 0000 0000 0000 0000 0000
//   1    1    0  0040_1000  0000 0000 0100 0000 0001 0000 0000 0000
//
//   0    0    0  0000_0000  0000 0000 0000 0000 0000 0000 0000 0000
//   0    1    0  0010_0000  0000 0000 0001 0000 0000 0000 0000 0000
//   0    2    0  0020_0000  0000 0000 0010 0000 0000 0000 0000 0000
//   0    3    0  0030_0000  0000 0000 0011 0000 0000 0000 0000 0000
//   1    0    0  0040_0000  0000 0000 0100 0000 0000 0000 0000 0000
//   1    1    0  0050_0000  0000 0000 0101 0000 0000 0000 0000 0000
//   1    2    0  0060_0000  0000 0000 0110 0000 0000 0000 0000 0000
//   1    3    0  0070_0000  0000 0000 0111 0000 0000 0000 0000 0000
//   2    0    0  0080_0000  0000 0000 1000 0000 0000 0000 0000 0000
//   2    1    0  0090_0000  0000 0000 1001 0000 0000 0000 0000 0000
//   2    2    0  00a0_0000  0000 0000 1010 0000 0000 0000 0000 0000
//   2    3    0  00b0_0000  0000 0000 1011 0000 0000 0000 0000 0000
//   3    0    0  00c0_0000  0000 0000 1100 0000 0000 0000 0000 0000
//   3    1    0  00d0_0000  0000 0000 1101 0000 0000 0000 0000 0000
//   3    2    0  00e0_0000  0000 0000 1110 0000 0000 0000 0000 0000
//   3    3    0  00f0_0000  0000 0000 1111 0000 0000 0000 0000 0000

// clion evaluate:
// PDX(0b11111111110000000000000000000000)
// PDX(0xfe000000)
//
// (gdb) p/t 0x80110000

// page directory index
#define PDX(va)         (((uint)(va) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(va)         (((uint)(va) >> PTXSHIFT) & 0x3FF)

// construct virtual address from indexes and offset
#define PGADDR(d, t, o) ((uint)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
#define NPDENTRIES      1024    // # directory entries per page directory
#define NPTENTRIES      1024    // # PTEs per page table
// 4-6 4.2 HIERARCHICAL PAGING STRUCTURES: AN OVERVIEW
// Every paging structure is 4096 Bytes ...
#define PGSIZE          4096    // bytes mapped by a page

#define PTXSHIFT        12      // offset of PTX in a linear address
#define PDXSHIFT        22      // offset of PDX in a linear address

// PGROUNDUP(4096) -> 4096
// PGROUNDUP(4097) -> 8192
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
// PGROUNDDOWN(4095) -> 0
// PGROUNDDOWN(4096) -> 4096
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// pde_t *pde = pgdir[PDE(va) (0-1024)]:
// 4B (32 bits)
// +--------------------+------------+
//  20                   12
//  PTE_ADDR             PTE_FLAGS
//  -> V2P(pgtab)        PTE_P | PTE_W | PTE_U
//
// pte_t *pgtab
// 4B (32 bits)
// +--------------------+------------+
//  20                   12
//  PTE_ADDR             PTE_FLAGS
//  -> pa                perm | PTE_P
//        pa | perm | PTE_P
//
// > The permissions here are overly generous, but they can
// > be further restricted by the permissions in the page table
// > entries, if necessary.

// Page table/directory entry flags.
#define PTE_P           0x001   // Present
#define PTE_W           0x002   // Writeable
#define PTE_U           0x004   // User
#define PTE_PS          0x080   // Page Size

// Address in page table or page directory entry
#define PTE_ADDR(pte)   ((uint)(pte) & ~0xFFF)
#define PTE_FLAGS(pte)  ((uint)(pte) &  0xFFF)

#ifndef __ASSEMBLER__
typedef uint pte_t;

// Task state segment format
struct taskstate {
  uint link;         // Old ts selector
  uint esp0;         // Stack pointers and segment selectors
  ushort ss0;        //   after an increase in privilege level
  ushort padding1;
  uint *esp1;
  ushort ss1;
  ushort padding2;
  uint *esp2;
  ushort ss2;
  ushort padding3;
  void *cr3;         // Page directory base
  uint *eip;         // Saved state from last task switch
  uint eflags;
  uint eax;          // More saved state (registers)
  uint ecx;
  uint edx;
  uint ebx;
  uint *esp;
  uint *ebp;
  uint esi;
  uint edi;
  ushort es;         // Even more saved state (segment selectors)
  ushort padding4;
  ushort cs;
  ushort padding5;
  ushort ss;
  ushort padding6;
  ushort ds;
  ushort padding7;
  ushort fs;
  ushort padding8;
  ushort gs;
  ushort padding9;
  ushort ldt;
  ushort padding10;
  ushort t;          // Trap on task switch
  ushort iomb;       // I/O map base address
};

// Gate descriptors for interrupts and traps
struct gatedesc {
  uint off_15_0 : 16;   // low 16 bits of offset in segment
  uint cs : 16;         // code segment selector
  uint args : 5;        // # args, 0 for interrupt/trap gates
  uint rsv1 : 3;        // reserved(should be zero I guess)
  uint type : 4;        // type(STS_{IG32,TG32})
  uint s : 1;           // must be 0 (system)
  uint dpl : 2;         // descriptor(meaning new) privilege level
  uint p : 1;           // Present
  uint off_31_16 : 16;  // high bits of offset in segment
};

// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
//   interrupt gate clears FL_IF, trap gate leaves FL_IF alone
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//        the privilege level required for software to invoke
//        this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, d)                \
{                                                         \
  (gate).off_15_0 = (uint)(off) & 0xffff;                \
  (gate).cs = (sel);                                      \
  (gate).args = 0;                                        \
  (gate).rsv1 = 0;                                        \
  (gate).type = (istrap) ? STS_TG32 : STS_IG32;           \
  (gate).s = 0;                                           \
  (gate).dpl = (d);                                       \
  (gate).p = 1;                                           \
  (gate).off_31_16 = (uint)(off) >> 16;                  \
}

#endif
