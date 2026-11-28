// Exercise Terry A. Davis's Adam/God/GodDoodle.HC.
U64 GodDoodleHash(U8 *data,I64 size)
{
  U64 hash=0xCBF29CE484222325;
  I64 i;
  for (i=0;i<size;i++)
    hash=(hash^data[i])*0x100000001B3;
  return hash;
}

U0 GodDoodleProbe()
{
  U8 *seed=CAlloc(650),*sprite,*digits="0123456789abcdef";
  U64 n=0x6A09E667F3BCC909,hash=0;
  I64 i,w=0,h=0,size=0,left=Fs->win_left,right=Fs->win_right,
      top=Fs->win_top,bottom=Fs->win_bottom,
      draw=Fs->draw_it,remaining=0,checks=0,f1=Fs->display_flags;
  CTaskSettings *settings=Fs->next_settings;
  CDoc *doc=DocPut;
  CDocBin *bin;
  for (i=0;i<640;i++) {
    n^=n<<13;
    n^=n>>7;
    n^=n<<17;
    seed[i]=digits[n&15];
  }
  MemCpy(seed+640," -gE0 ",7);
  sprite=GodDoodleSprite(seed);
  if (sprite && sprite[0]==SPT_BITMAP) {
    w=*(sprite+9)(I32 *);
    h=*(sprite+13)(I32 *);
    size=SpriteSize(sprite);
    if (w>0 && h>0 && size==18+((w+7)&~7)*h && !sprite[size-1]) {
      hash=GodDoodleHash(sprite,size);
      checks|=1;
    }
  }
  Free(sprite);
  remaining=FifoU8Cnt(god.doodle_fifo);
  if (god.doodle_done && !god.doodle_dc && Fs->win_left==left &&
      Fs->win_right==right && Fs->win_top==top && Fs->win_bottom==bottom &&
      Fs->display_flags==f1 && Fs->draw_it==draw && Fs->next_settings==settings)
    checks|=2;

  GodDoodle(seed);
  bin=doc->bin_head.last;
  if (bin!=&doc->bin_head && bin->size==size && size>0 &&
      GodDoodleHash(bin->data,size)==hash &&
      FifoU8Cnt(god.doodle_fifo)==remaining && !god.doodle_dc)
    checks|=4;
  DocClear(doc,TRUE);
  DocPutKey(doc,0,SC_F6|SCF_SHIFT|SCF_KEY_DESC);
  if (!StrCmp(keydev.desc,"Cmd /God Doodle"))
    checks|=8;
  Free(seed);
  "Doodle bitmap %d %d %d %016X\n",w,h,size,hash;
  "Doodle checks %X %d\n",checks,remaining;
}

GodDoodleProbe;
