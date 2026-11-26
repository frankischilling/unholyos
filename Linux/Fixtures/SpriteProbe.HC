U0 SpriteProbe()
{
  U8 *sprite=$IB,"External",BI=1,BP="::/Demo/Graphics/SpritePut.HC,2"$;
  CDC *dc=DCNew(160,100),*clip=DCNew(32,40);
  I64 x,y,color,full_diff=0,clip_diff=0;
  DCFill(dc,WHITE);
  DCFill(clip,BLUE);
  dc->color=RED;
  Sprite3(dc,4,3,0,sprite);
  Sprite3(clip,-11,-7,0,sprite,TRUE);
  for (y=0;y<100;y++)
    for (x=0;x<160;x++) {
      color=TRANSPARENT;
      if (4<=x && x<146 && 3<=y && y<90)
        color=sprite[17+(y-3)*144+x-4];
      if (color==TRANSPARENT)
        color=WHITE;
      if (GrPeek(dc,x,y)!=color)
        full_diff++;
    }
  for (y=0;y<40;y++)
    for (x=0;x<32;x++) {
      color=sprite[17+(y+7)*144+x+11];
      if (color==TRANSPARENT)
        color=BLUE;
      if (GrPeek(clip,x,y)!=color)
        clip_diff++;
    }
  "sprite bitmap %d %d %d %d\n",SpriteSize(sprite),full_diff,clip_diff,dc->color;
  DCDel(clip);
  DCDel(dc);
}
SpriteProbe;
