// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mp.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;

static uchar
sum(uchar *addr, int len)
{
  int i, sum;

  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp*
mpsearch1(uint a, int len)
{
  uchar *e, *p, *addr;

  addr = P2V(a);
  e = addr+len;
  for(p = addr; p < e; p += sizeof(struct mp))
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
//                              ^ F! TODO: PR
static struct mp*
mpsearch(void)
{
  uchar *bda;
  uint p;
  struct mp *mp;

  // https://wiki.osdev.org/Memory_Map_(x86)
  // 0x400 BDA (BIOS data area)
  // Extended BIOS Data Area (EBDA)
  // 0x000F0000 - 0x000FFFFF 64KiB ROM  Motherboard BIOS
  //
  // 0x0400 (4 words) IO ports for COM1-COM4
  // (gdb) x/4xh 0x80000400
  // 0x80000400:	0x03f8	0x0000	0x0000	0x0000

  bda = (uchar *) P2V(0x400);
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){
    if((mp = mpsearch1(p, 1024)))
    {
      notreached();
      return mp;
    }
  } else {
    notreached();
    p = ((bda[0x14]<<8)|bda[0x13])*1024;
    if((mp = mpsearch1(p-1024, 1024)))
      return mp;
  }
  return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mpconf*
mpconfig(struct mp **pmp)
{
  struct mpconf *conf;
  struct mp *mp;

  if((mp = mpsearch()) == 0 || mp->physaddr == 0)
    return 0;
  conf = (struct mpconf*) P2V((uint) mp->physaddr);
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

void
mpinit(void)
{
  uchar *p, *e;
  int ismp;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  if((conf = mpconfig(&mp)) == 0)
    panic("Expect to run on an SMP");

  //                                 size
  // sizeof(*conf) 44
  // 0x800f5a90 &conf->signature[4]     4  "PCMP"                   // "PCMP"
  // 0x800f5a94 &conf->length           2  228                      // total table length
  // 0x800f5a96 &conf->version          1  4                        // [14]
  // 0x800f5a97 &conf->checksum         1  53                       // all bytes must add up to 0
  // 0x800f5a98 &conf->product[20]     20  "BOCHSCPU0.         "    // product id
  // 0x800f5aac &conf->oemtable         4  -> NULL                  // OEM table pointer
  // 0x800f5ab0 &conf->oemlength        2  0                        // OEM table length
  // 0x800f5ab2 &conf->entry            2  20                       // entry count
  // 0x800f5ab4 &conf->lapicaddr        4  0xfee00000 -> 0          // address of local APIC
  // 0x800f5ab8 &conf->xlength          2  0                        // extended table length
  // 0x800f5aba &conf->xchecksum        1  0                        // extended table checksum
  // 0x800f5abb &conf->reserved         1  0
  // sizeof(*proc) 20
  // cpu0
  // 0x800f5abc &proc->type             1  MPPROC(0)  conf+1 (+45)  // entry type (0)
  // 0x800f5abd &proc->apicid           1  0                        // local APIC id
  // 0x800f5abe &proc->version          1  20                       // local APIC verison
  // 0x800f5abf &proc->flags            1  3                        // CPU flags
  // 0x800f5ac0 &proc->signature        4  99 6 0 0                 // This proc is the bootstrap processor.
  // 0x800f5ac4 &proc->feature          4  0x0781abfd               // CPU signature
  // 0x800f5ac8 &proc->reserved         8  0 0 0 0 0 0 0 0          // feature flags from CPUID instruction
  // cpu1
  // 0x800f5ad0 &proc->type             1  same
  // 0x800f____ &proc->apicid           1  1
  // 0x800f____ &proc->version          1  same
  // 0x800f____ &proc->flags            1  1
  // 0x800f____ &proc->signature        4  99 6 0 0
  // 0x800f____ &proc->feature          4  same
  // 0x800f____ &proc->reserved         8  0 0 0 0 0 0 0 0
  // 0x800f5ae4 MPBUS(1)
  // 0x800f5aec MPBUS(1)
  // sizeof(*ioapic) 8
  // 0x800f5af4 &ioapic->type           1 MPIOAPIC(2)               // entry type (2)
  // 0x800f____ &ioapic->apicno         1 0                         // I/O APIC id
  // 0x800f____ &ioapic->version        1 17                        // I/O APIC version
  // 0x800f____ &ioapic->flags          1 1                         // I/O APIC flags
  // 0x800f____ &ioapic->addr           4 0xfec00000 -> 0           // I/O APIC address
  // 0x800f5afc MPIOINTR(3)
  // 0x800f5b04 MPIOINTR(3)
  // 0x800f5b0c MPIOINTR(3)
  // 0x800f5b14 MPIOINTR(3)
  // 0x800f5b1c MPIOINTR(3)
  // 0x800f5b24 MPIOINTR(3)
  // 0x800f5b2c MPIOINTR(3)
  // 0x800f5b34 MPIOINTR(3)
  // 0x800f5b3c MPIOINTR(3)
  // 0x800f5b44 MPIOINTR(3)
  // 0x800f5b4c MPIOINTR(3)
  // 0x800f5b54 MPIOINTR(3)
  // 0x800f5b5c MPIOINTR(3)
  // 0x800f5b64 MPLINTR(4)
  // 0x800f5b6c MPLINTR(4)
  // 0x800f5b74 0     (uchar*)conf + conf->length(228)  e

  ismp = 1;
  lapic = (uint*)conf->lapicaddr;
  for(p=(uchar*)(conf+1), e=(uchar*)conf+conf->length; p<e; ){
    switch(*p){
    case MPPROC:
      proc = (struct mpproc*)p;
      if(ncpu < NCPU) {
        cpus[ncpu].apicid = proc->apicid;  // apicid may differ from ncpu
        ncpu++;
      }
      p += sizeof(struct mpproc);
      continue;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      ioapicid = ioapic->apicno;
      p += sizeof(struct mpioapic);
      continue;
    case MPBUS:
      p += 8;
      continue;
    case MPIOINTR:
      p += 8;
      continue;
    case MPLINTR:
      p += 8;
      continue;
    default:
      notreached();
      ismp = 0;
      break;
    }
  }
  if(!ismp)
    panic("Didn't find a suitable machine");

  if(mp->imcrp){
    notreached();
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
