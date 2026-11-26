extern U0 throw(I64 ch=0,Bool no_log=FALSE);

U0 ThrowFromCursor(CDC *dc,I64 x,I64 y)
{
  gr.fp_draw_ms=&DrawStdMs;
  throw('Cursor',TRUE);
}

U0 CursorExcept()
{
  gr.fp_draw_ms=&ThrowFromCursor;
  Sleep(110);
  "cursor exception %d\n",gr.fp_draw_ms==&DrawStdMs;
}

CursorExcept;
