// Inspect native member records after the register allocation pass.
I64 RegNativeLeaf(I64 reg x)
{
  return x*2+1;
}

I64 RegNativeAuto(I64 reg arg,I64 noreg stack_arg)
{
  I64 reg hot=arg+1;
  I64 noreg cold=stack_arg;
  hot+=RegNativeLeaf(arg);
  hot+=RegNativeLeaf(cold);
  return hot+cold;
}

I64 RegNativeAddress(I64 reg arg)
{
  I64 reg escaped=arg;
  I64 *p=&escaped;
  *p+=3;
  return escaped;
}

I64 RegNativeExplicit(I64 value)
{
  I64 reg R15 keep=value+4;
  return keep+RegNativeLeaf(value);
}

// Native frame slots are allocated downward, one I64 per declaration.
// Escaping the noreg member exposes the adjacent register candidate too.
I64 RegNativeCrossSlot()
{
  I64 noreg anchor=1;
  I64 reg target=2;
  (&anchor)[-1]=9;
  return target;
}

Bool RegNativePromoted(CHashFun *fn,CMemberLst *member)
{
  return fn && member && member->reg>=0 && member->reg<16 &&
      (fn->used_reg_mask&(1<<member->reg));
}

U0 RegisterAllocationNativeProbe()
{
  CHashFun *fn;
  CMemberLst *arg,*stack_arg,*hot,*cold,*escaped,*keep;
  I64 res=0;
  fn=HashFind("RegNativeAuto",Fs->hash_table,HTT_FUN);
  if (fn) {
    arg=MemberFind("arg",fn);
    stack_arg=MemberFind("stack_arg",fn);
    hot=MemberFind("hot",fn);
    cold=MemberFind("cold",fn);
    if (RegNativePromoted(fn,arg) &&
        RegNativePromoted(fn,hot) && arg->reg!=hot->reg) res|=1;
    if (stack_arg && stack_arg->reg==32 &&
        cold && cold->reg==32) res|=2;
  }
  fn=HashFind("RegNativeAddress",Fs->hash_table,HTT_FUN);
  if (fn) {
    escaped=MemberFind("escaped",fn);
    if (escaped && !RegNativePromoted(fn,escaped) &&
        (escaped->reg<0 || escaped->reg>=16)) res|=4;
  }
  fn=HashFind("RegNativeExplicit",Fs->hash_table,HTT_FUN);
  if (fn) {
    keep=MemberFind("keep",fn);
    if (RegNativePromoted(fn,keep) && keep->reg==15) res|=8;
  }
  if (RegNativeAuto(5,3)==27 &&
      RegNativeAddress(7)==10 && RegNativeExplicit(6)==23) res|=16;
  fn=HashFind("RegNativeCrossSlot",Fs->hash_table,HTT_FUN);
  if (fn) {
    CMemberLst *anchor=MemberFind("anchor",fn),*target=MemberFind("target",fn);
    if (anchor && target && anchor->offset-target->offset==8 &&
        !(fn->used_reg_mask&0xF000) && !RegNativePromoted(fn,target) &&
        RegNativeCrossSlot==9) res|=32;
  }
  "Register allocation native %X\n",res;
}

RegisterAllocationNativeProbe;
