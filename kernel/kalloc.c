// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem_list;

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem_list.lock[i], "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  struct run *r;
  int id = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem_list.lock[id]);
  r->next = kmem_list.freelist[id];
  kmem_list.freelist[id] = r;
  release(&kmem_list.lock[id]);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  struct run *r;
  // Rertun current core number
  int id = cpuid();

  acquire(&kmem_list.lock[id]);
  r = kmem_list.freelist[id];

  if(r) {
    kmem_list.freelist[id] = r->next;
  }
  release(&kmem_list.lock[id]);

  if (!r) {
    // When freelist is empty, steal other CPU's free list
    for (int i = 0; i < NCPU; i++) {
      if (i == id) {
        continue;
      }
      acquire(&kmem_list.lock[i]);
      r = kmem_list.freelist[i];
      if (r) {
        kmem_list.freelist[i] = r->next;
      }
      release(&kmem_list.lock[i]);
      if (r) 
        break;
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
