#include "../KernelB.hpp"

// The LRU sentinel is just the header; hash sentinels occupy two pointers.
// Access their link slots without treating either allocation as a full record.
static CCacheBlk *&CacheLink(CCacheBlk *entry,size_t offset)
{
  return *reinterpret_cast<CCacheBlk **>(
      reinterpret_cast<uintptr_t>(entry)+offset);
}

static CCacheBlk *&NextLRU(CCacheBlk *entry)
{
  return CacheLink(entry,offsetof(CCacheBlk,next_lru));
}

static CCacheBlk *&LastLRU(CCacheBlk *entry)
{
  return CacheLink(entry,offsetof(CCacheBlk,last_lru));
}

static CCacheBlk *&NextHash(CCacheBlk *entry)
{
  return CacheLink(entry,offsetof(CCacheBlk,next_hash));
}

static CCacheBlk *&LastHash(CCacheBlk *entry)
{
  return CacheLink(entry,offsetof(CCacheBlk,last_hash));
}

static void CacheLRUIns(CCacheBlk *entry,CCacheBlk *last)
{
  auto *next=NextLRU(last);
  entry->next_lru=next;
  entry->last_lru=last;
  NextLRU(last)=LastLRU(next)=entry;
}

static CCacheBlk *CacheHashHead(CCacheBlk **table,size_t i)
{
  return reinterpret_cast<CCacheBlk *>(
      reinterpret_cast<uintptr_t>(table+2*i)-offsetof(CCacheBlk,next_hash));
}

extern "C" void DskCacheInitCpp(int64_t size_in_U8s)
{
  CMemLock lock(sys_semas[SEMA_DSK_CACHE].flags);
  Free(blkdev.cache_ctrl);
  Free(blkdev.cache_base);
  Free(blkdev.cache_hash_table);
  blkdev.cache_ctrl=nullptr;
  blkdev.cache_base=nullptr;
  blkdev.cache_hash_table=nullptr;
  if (size_in_U8s<0x2000 || !adam_task)
    return;
  auto *hc=adam_task->data_heap;
  auto *ctrl=reinterpret_cast<CCacheBlk *>(CAlloc(offsetof(CCacheBlk,body),hc));
  auto *base=reinterpret_cast<CCacheBlk *>(MAlloc(size_in_U8s,hc));
  auto **table=reinterpret_cast<CCacheBlk **>(
      MAlloc(DSK_CACHE_HASH_SIZE*2*sizeof(CCacheBlk *),hc));
  if (!ctrl || !base || !table) {
    Free(ctrl);
    Free(base);
    Free(table);
    return;
  }
  size_t cnt=MSize(base)/sizeof(CCacheBlk);
  NextLRU(ctrl)=LastLRU(ctrl)=ctrl;
  for (size_t i=0;i<cnt;i++) {
    auto *tmpc=base+i;
    CacheLRUIns(tmpc,LastLRU(ctrl));
    tmpc->next_hash=tmpc->last_hash=tmpc;
    tmpc->dv=nullptr;
    tmpc->blk=0;
  }
  for (size_t i=0;i<DSK_CACHE_HASH_SIZE;i++) {
    auto *tmpc=CacheHashHead(table,i);
    NextHash(tmpc)=LastHash(tmpc)=tmpc;
  }
  blkdev.cache_size=cnt*BLK_SIZE;
  blkdev.cache_ctrl=ctrl;
  blkdev.cache_hash_table=table;
  blkdev.cache_base=base;
}

static CCacheBlk *DskCacheHash(int64_t blk)
{
  return CacheHashHead(blkdev.cache_hash_table,
                       uint64_t(blk)&(DSK_CACHE_HASH_SIZE-1));
}

static void DskCacheQueRem(CCacheBlk *tmpc)
{
  NextLRU(tmpc->last_lru)=tmpc->next_lru;
  LastLRU(tmpc->next_lru)=tmpc->last_lru;
  NextHash(tmpc->last_hash)=tmpc->next_hash;
  LastHash(tmpc->next_hash)=tmpc->last_hash;
}

static void DskCacheQueIns(CCacheBlk *tmpc)
{
  CacheLRUIns(tmpc,LastLRU(blkdev.cache_ctrl));
  auto *tmp_l=DskCacheHash(tmpc->blk),*tmp_n=NextHash(tmp_l);
  tmpc->last_hash=tmp_l;
  tmpc->next_hash=tmp_n;
  NextHash(tmp_l)=LastHash(tmp_n)=tmpc;
}

static CCacheBlk *DskCacheFind(CDrv *dv,int64_t blk)
{
  auto *tmpc1=DskCacheHash(blk);
  for (auto *tmpc=NextHash(tmpc1);tmpc!=tmpc1;tmpc=tmpc->next_hash)
    if (tmpc->dv==dv && tmpc->blk==blk)
      return tmpc;
  return nullptr;
}

static bool CacheRange(int64_t blk,int64_t cnt)
{
  return cnt>0 && cnt<=INT64_MAX/int64_t(BLK_SIZE) && blk<=INT64_MAX-cnt;
}

extern "C" void DskCacheAddCpp(CDrv *dv,const uint8_t *buf,int64_t blk,
                                int64_t cnt)
{
  if (!dv || !buf || !CacheRange(blk,cnt))
    return;
  CMemLock lock(sys_semas[SEMA_DSK_CACHE].flags);
  if (!blkdev.cache_base)
    return;
  while (cnt-->0) {
    auto *tmpc=DskCacheFind(dv,blk);
    if (!tmpc)
      tmpc=NextLRU(blkdev.cache_ctrl);
    DskCacheQueRem(tmpc);
    MemCpy(tmpc->body,buf,BLK_SIZE);
    tmpc->dv=dv;
    tmpc->blk=blk;
    DskCacheQueIns(tmpc);
    blk++;
    buf+=BLK_SIZE;
  }
}

static void DskCacheInvalidate(CDrv *dv,CBlkDev *bd)
{
  CMemLock lock(sys_semas[SEMA_DSK_CACHE].flags);
  if (!blkdev.cache_base)
    return;
  auto *ctrl=blkdev.cache_ctrl;
  for (auto *tmpc=LastLRU(ctrl);tmpc!=ctrl;) {
    auto *tmpc1=tmpc->last_lru;
    if (tmpc->dv && (dv ? tmpc->dv==dv : tmpc->dv->bd==bd)) {
      DskCacheQueRem(tmpc);
      tmpc->dv=nullptr;
      tmpc->blk=0;
      tmpc->next_hash=tmpc->last_hash=tmpc;
      CacheLRUIns(tmpc,LastLRU(ctrl));
    }
    tmpc=tmpc1;
  }
}

extern "C" void DskCacheInvalidate2Cpp(CDrv *dv)
{
  if (dv)
    DskCacheInvalidate(dv,nullptr);
}

void DskCacheInvalidateDevice(CBlkDev *bd)
{
  if (bd)
    DskCacheInvalidate(nullptr,bd);
}

// Raw byte writes include partial sectors and may affect several drive mappings.
// Update every cached copy without changing its LRU position. A failed write
// may have reached the media in part, so discard affected cache entries.
void DskCacheWriteThrough(CBlkDev *bd,const uint8_t *buf,uint64_t pos,
                          size_t bytes,bool okay)
{
  if (!bytes) return;
  CMemLock lock(sys_semas[SEMA_DSK_CACHE].flags);
  if (!blkdev.cache_base) return;
  auto *ctrl=blkdev.cache_ctrl;
  for (auto *tmpc=LastLRU(ctrl);tmpc!=ctrl;) {
    auto *prev=tmpc->last_lru;
    if (tmpc->dv && tmpc->dv->bd==bd && tmpc->blk>=0 &&
        uint64_t(tmpc->blk)<=UINT64_MAX/BLK_SIZE) {
      uint64_t start=uint64_t(tmpc->blk)*BLK_SIZE;
      if ((start>=pos && start-pos<bytes) ||
          (start<pos && pos-start<BLK_SIZE)) {
        if (okay) {
          size_t src=start>pos?start-pos:0;
          size_t dst=pos>start?pos-start:0;
          size_t cnt=bytes-src;
          if (cnt>BLK_SIZE-dst) cnt=BLK_SIZE-dst;
          MemCpy(tmpc->body+dst,buf+src,cnt);
        } else {
          DskCacheQueRem(tmpc);
          tmpc->dv=nullptr;
          tmpc->blk=0;
          tmpc->next_hash=tmpc->last_hash=tmpc;
          CacheLRUIns(tmpc,LastLRU(ctrl));
        }
      }
    }
    tmpc=prev;
  }
}

extern "C" void RCacheCpp(CDrv *dv,uint8_t **_buf,int64_t *_blk,
                           int64_t *_cnt)
{
  if (!dv || !_buf || !*_buf || !_blk || !_cnt ||
      !CacheRange(*_blk,*_cnt))
    return;
  CMemLock lock(sys_semas[SEMA_DSK_CACHE].flags);
  if (!blkdev.cache_base)
    return;
  // Fetch leading blocks, then trailing blocks. Reads do not change LRU order.
  while (*_cnt>0) {
    auto *tmpc=DskCacheFind(dv,*_blk);
    if (!tmpc)
      break;
    MemCpy(*_buf,tmpc->body,BLK_SIZE);
    (*_cnt)--;
    *_buf+=BLK_SIZE;
    (*_blk)++;
  }
  while (*_cnt>0) {
    auto *tmpc=DskCacheFind(dv,*_blk+*_cnt-1);
    if (!tmpc)
      break;
    MemCpy(*_buf+(*_cnt-1)*BLK_SIZE,tmpc->body,BLK_SIZE);
    (*_cnt)--;
  }
}
