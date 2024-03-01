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
} kmem[NCPU]; // now each hart will have its kmem.

void
kinit()
{
  for (int i = 0; i < NCPU; i ++)
    initlock(&kmem[i].lock, "kmem");
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  //intr off to get hartid.
  push_off();
  int cpu = cpuid();
  pop_off();
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int cpu = cpuid();
  pop_off();
  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  else {
    // try to steal from other hart's list.
    for (int i = 0; i < NCPU; i ++) {
      if (i != cpu) {
        acquire(&kmem[i].lock);
        struct run *rr = kmem[i].freelist;
        if (rr) {
          kmem[i].freelist = rr->next;
          rr->next = 0;
          if (!r) {
            kmem[cpu].freelist = rr;
            r = kmem[cpu].freelist;
            // current freelist empty, allocate it a new page.
          } else {
            rr->next = r;
            kmem[cpu].freelist = rr;
            r = kmem[cpu].freelist;
          }
          // stealed only one page from an avaiable freelist.
        }
        release(&kmem[i].lock);
      }
    }
    r = kmem[cpu].freelist;
    if(r)
      kmem[cpu].freelist = r->next;
  }
  release(&kmem[cpu].lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
