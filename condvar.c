#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "mmu.h"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.h"

void
initcondvar(struct condvar *cv, char *n)
{
  initlock(&cv->lock, n);
}

void
cv_sleep(struct condvar *cv, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire cv_lock to avoid sleep/wakeup race
  acquire(&cv->lock); 

  release(lk);

  if (cv->waiters != 0)
    panic("cv_sleep\n");

  cv->waiters = proc;  // XXX should be queue

  acquire(&proc->lock);

  release(&cv->lock);

  proc->state = SLEEPING;

  sched();

  release(&proc->lock);

  // Reacquire original lock.
  acquire(lk);
}

void
cv_wakeup(struct condvar *cv)
{
  acquire(&cv->lock);
  if (cv->waiters != 0) {
    addrun(cv->waiters);
    cv->waiters = 0;
  }
  release(&cv->lock);
}