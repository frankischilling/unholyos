// Test-only observer for the unmodified CharDemo.HC.
I64 *cgpan=CAlloc(32);

U0 CGWatch(I64)
{
  while (!cgpan[2]) {
    Refresh;
    cgpan[3]++;
    if (gr.hide_row && gr.pan_text_x<7)
      cgpan[0]++;
    if (gr.hide_col && gr.pan_text_y<7)
      cgpan[1]++;
  }
  cgpan[2]=2;
}

U0 CGRun()
{
  CTask *watch=Spawn(&CGWatch,0,"Pan Watch",,Fs);
  if (!watch)
    throw('CGWatch');
  while (!cgpan[3])
    Yield;
  Seed(12345);
  DocClear(,1);
  ExeFile("::/Demo/Games/CharDemo.HC");
  cgpan[2]=1;
  while (TaskValidate(watch))
    Yield;
  "CGWATCH %d\n",cgpan[2];
}

U0 CGDone()
{
  Free(cgpan);
  cgpan=NULL;
  "CGDONE\n";
}

"CGPAN:%X:END\n",cgpan;
