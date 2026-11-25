#exe {
  #exe { StreamPrint("I64 StreamSeed() { return %d; }\n",7); };
  StreamPrint("#define STREAM_BASE %d\n",StreamSeed*6);
  StreamPrint("I64 StreamGenerated(I64 n) { return STREAM_BASE+n; }\n");
};

I64 StreamOuter(I64 n)
{
  #exe {
    StreamPrint("I64 add=%d;\n",StreamExePrint("STREAM_BASE;"));
  };
  return add+StreamGenerated(n);
}

I64 stream_loaded=0, stream_result, stream_suppressed;
stream_suppressed=ExeFile("/StreamNested",0x40);
"load %d %d\n",stream_suppressed,stream_loaded;
stream_result=ExeFile("/StreamNested");
"stream %d %d %d %d\n",StreamOuter(3),stream_result,stream_loaded,StreamGenerated(4);
StreamOuter(3)+stream_result;
