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

#define N_BUCKETS 13

// Helper function for generating hash
uint
hash(uint dev, uint blockno){
  return ((dev << 27) ^ blockno) % N_BUCKETS;
}

struct {
  struct spinlock evict_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bufmap[N_BUCKETS];
  struct spinlock bufmaplock[N_BUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < N_BUCKETS; i++) {
    initlock(&bcache.bufmaplock[i], "bcache");
    bcache.bufmap[i].next = 0;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->lastuse = 0;
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.evict_lock, "evict lock");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint bucket = hash(dev, blockno);
  acquire(&bcache.bufmaplock[bucket]);

  // Is the block already cached?
  for(b = bcache.bufmap[bucket].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmaplock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Acquire evict lock to avoid racing in eviction
  release(&bcache.bufmaplock[bucket]);
  acquire(&bcache.evict_lock);

  for(b = bcache.bufmap[bucket].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bufmaplock[bucket]);
      b->refcnt++;
      release(&bcache.bufmaplock[bucket]);
      release(&bcache.evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Use buffer before lru buffer to avoid using doubly linked list
  struct buf *buf_before_lru = 0;
  uint lru_buf_bucket = -1;
  for (int i = 0; i < N_BUCKETS; i++) {
    acquire(&bcache.bufmaplock[i]);
    int new_lru_found = 0;
    for (b = &bcache.bufmap[i]; b->next ; b = b->next) {
      if (b->next->refcnt == 0 && (!buf_before_lru || b->next->lastuse < buf_before_lru->next->lastuse)) {
        buf_before_lru = b;
        new_lru_found = 1;
      }
    }
    // Whether find a new lru in current bucket
    if (!new_lru_found) {
      release(&bcache.bufmaplock[i]);
    } else {
      if (lru_buf_bucket != -1) {
        release(&bcache.bufmaplock[lru_buf_bucket]);
      }
      lru_buf_bucket = i;
    }
  }

  if (!buf_before_lru) {
    panic("bget: no buffers");
  }

  b = buf_before_lru->next;

  if (lru_buf_bucket != bucket) {
    // Remove old buffer from linked list
    buf_before_lru->next = buf_before_lru->next->next;
    release(&bcache.bufmaplock[lru_buf_bucket]);
    acquire(&bcache.bufmaplock[bucket]);
    b->next = bcache.bufmap[bucket].next;
    bcache.bufmap[bucket].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.bufmaplock[bucket]);
  release(&bcache.evict_lock);
  acquiresleep(&b->lock);
  return b;
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

  uint bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bufmaplock[bucket]);
  b->refcnt--;

  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }
  
  release(&bcache.bufmaplock[bucket]);
}

void
bpin(struct buf *b) {
  uint bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bufmaplock[bucket]);
  b->refcnt++;
  release(&bcache.bufmaplock[bucket]);
}

void
bunpin(struct buf *b) {
  uint bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bufmaplock[bucket]);
  b->refcnt--;
  release(&bcache.bufmaplock[bucket]);
}


