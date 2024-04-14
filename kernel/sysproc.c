#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // Get the arguments
  uint64 va;
  argaddr(0, &va);

  uint64 abits_addr;
  argaddr(2, &abits_addr);

  int n_pages;
  argint(1, &n_pages);

  // Temporary buffer to store bits
  unsigned int abits = 0;

  // Retrieve page table
  pagetable_t pagetable = myproc()->pagetable;

  for (int i = 0; i < n_pages; i++) {
    uint64 a = va + i * PGSIZE;
    pte_t *pte = walk(pagetable, a, 0);
    // Write pages that have been accessed
    if (*pte & PTE_A) {
      abits |= 1 << i;
      // Reset the access bit to 0
      *pte &= ~PTE_A;
    }
  }

  // Copy out result bits result
  if (copyout(pagetable, abits_addr, (char *)&abits, sizeof abits) < 0) {
    return -1;
  };

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
