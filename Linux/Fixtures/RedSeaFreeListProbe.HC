// Reference RedSea free-extent queue, best-fit selection and format APIs.
class CProbeFreeLst {
  CProbeFreeLst *next,*last;
  I64 start,size;
};

U0 RedSeaFreeListProbe()
{
  CBlkDev *bd=NULL;
  CDrv *dv;
  CProbeFreeLst *head,*p;
  U8 letter='B',*saved=DirCur;
  I64 a,b,c,d,e,initial,res=0;
  U0 (*free_list)(CDrv *drive),(*build_list)(CDrv *drive);
  I64 (*alloc_clus)(CDrv *drive,I64 count);
  U0 (*free_clus)(CDrv *drive,I64 start,I64 count);
  Bool (*validate)(U8 drv_let);
  U0 (*fmt)(U8 drv_let,Bool quick);
  free_list=&RedSeaFreeFreeLst;
  build_list=&RedSeaFreeLstBuild;
  alloc_clus=&RedSeaAllocClus;
  free_clus=&RedSeaFreeClus;
  validate=&RedSeaValidate;
  fmt=&RedSeaFmt;
  if (Let2Drv(letter,FALSE)) letter='A';
  if (Let2Drv(letter,FALSE) || !saved) goto done;
  bd=BlkDevNextFreeSlot(letter,BDT_RAM);
  bd->max_blk=255;
  if (BlkDevAdd(bd,,FALSE,TRUE)!=1) goto done;
  dv=Let2Drv(letter,FALSE);
  if (!dv || !(*validate)(letter)) goto done;
  head=&dv->next_free;
  (*free_list)(dv);
  if (!dv->next_free) res|=1;
  (*build_list)(dv);
  p=dv->next_free;
  if (p && p!=head && p->last==head && p->next==head &&
      dv->last_free==p && p->start==dv->root_clus+1 &&
      p->size==dv->drv_offset+dv->size-p->start) res|=2;
  if (!p || p==head) goto done;
  initial=p->start;
  a=(*alloc_clus)(dv,3);
  b=(*alloc_clus)(dv,2);
  c=(*alloc_clus)(dv,4);
  p=dv->next_free;
  if (a==initial && b==a+3 && c==b+2 && p->start==c+4) res|=4;
  (*free_clus)(dv,a,3);
  p=dv->last_free;
  if (p->start==a && p->size==3) res|=8;
  d=(*alloc_clus)(dv,2);
  e=(*alloc_clus)(dv,1);
  if (d==a && e==a+2 && dv->next_free==dv->last_free) res|=16;
  (*free_clus)(dv,b,2);
  (*free_clus)(dv,c,4);
  (*free_list)(dv);
  (*build_list)(dv);
  p=dv->next_free;
  if (p==dv->last_free && p->start==b) res|=32;
  (*fmt)(letter,TRUE);
  (*build_list)(dv);
  p=dv->next_free;
  if ((*validate)(letter) && p->start==dv->root_clus+1 &&
      dv->next_free==dv->last_free) res|=64;
done:
  if (saved) Cd(saved);
  if (bd && bd->bd_signature==BD_SIGNATURE_VAL) BlkDevDel(bd);
  Free(saved);
  "RedSea free list %X\n",res;
}

RedSeaFreeListProbe;
