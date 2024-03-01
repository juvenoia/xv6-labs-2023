// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
//  struct buf head;
  struct buf hs[13];
  struct spinlock hsl[13];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for (int i = 0; i < 13; i ++) {
    bcache.hs[i].prev = &bcache.hs[i];
    bcache.hs[i].next = &bcache.hs[i];
    initlock(&bcache.hsl[i], "bcache");
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hs[0].next;
    b->prev = &bcache.hs[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hs[0].next->prev = b;
    bcache.hs[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //acquire(&bcache.lock);
  struct buf *b;
  uint id = blockno % 13;
  acquire(&bcache.hsl[id]);
  // this ensures no two programs are changing paralleling.

  // Is the block already cached?
//  for(b = bcache.head.next; b != &bcache.head; b = b->next){
//    if(b->dev == dev && b->blockno == blockno){
//      b->refcnt++;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
//  HERE WE USE A HASHTABLE TO DOWN THE PRESSURE, COMPARED TO ONLY ONE BCACHE.
//  I.E. CACHE ARE SPLITTED INTO DIFFERENT ONES.
  //printf("%d bucket is called, which is block %d\n", blockno % 13, blockno);
  for (b = bcache.hs[id].next; b != &bcache.hs[id]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      //printf("!!!!!!!!!!!we found the block in the memory!!!!!!!!!!!!\n");
      b->refcnt ++;
      release(&bcache.hsl[id]);
      acquiresleep(&b->lock);
      //release(&bcache.lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
//  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//    if(b->refcnt == 0) {
//      b->dev = dev;
//      b->blockno = blockno;
//      b->valid = 0;
//      b->refcnt = 1;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
// SEQUENTIAL FIND is OK.
// such reading to b is undefined(i.e, not locked. only those
// on the linkedlist could be locked!)
  // this do not need block since acquire turned off the intr?

  // try to find a blank bracket.

  for (b = bcache.hs[id].next; b != &bcache.hs[id]; b = b->next) {
    if (b->refcnt == 0) {
      //printf("!!!!!!!!!!!we found the block in the memory!!!!!!!!!!!!\n");
      b->refcnt = 1;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      release(&bcache.hsl[id]);
      acquiresleep(&b->lock);
      //release(&bcache.lock);
      return b;
    }
  }
  for (int k = 0; k < 13; k ++) {
    if (k == id)
      continue;
    acquire(&bcache.hsl[k]);
    for (b = bcache.hs[k].next; b != &bcache.hs[k]; b = b->next) {
      if (b->refcnt == 0) {
        //printf("!!!!!!!!!!!we found the block in the memory!!!!!!!!!!!!\n");
        b->refcnt = 1;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;

        b->prev->next = b->next;
        b->next->prev = b->prev;

        b->next = bcache.hs[id].next;
        b->prev = &bcache.hs[id];
        bcache.hs[id].next->prev = b;
        bcache.hs[id].next = b;

        release(&bcache.hsl[k]);
        release(&bcache.hsl[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.hsl[k]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  uint id = (b->blockno) % 13;
  acquire(&bcache.hsl[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    //printf("%d block is removed at bucket %d\n", blockno, blockno % 13);
    // no one is waiting for it.
//    b->dev = 0;
//    b->blockno = 0;
//    b->next = bcache.head.next;
//    b->prev = &bcache.head;
//    bcache.head.next->prev = b;
//    bcache.head.next = b;
    // release the block in the hash list.
  }
  release(&bcache.hsl[id]);
}

void
bpin(struct buf *b) {
  uint id = (b->blockno) % 13;
  acquire(&bcache.hsl[id]);
  b->refcnt++;
  release(&bcache.hsl[id]);
}

void
bunpin(struct buf *b) {
  uint id = (b->blockno) % 13;
  acquire(&bcache.hsl[id]);
  b->refcnt--;
  release(&bcache.hsl[id]);
}


