// Check selected machine-code operands in small, known JIT functions.
// Keep the frame slots unpromoted so the test covers memory lowering.
I64 AddressNativeFrame(I64 noreg seed)
{
  I64 noreg value=seed;
  value+=7;
  return value;
}

I64 AddressNativeFar(I64 noreg seed)
{
  U8 bytes[160];
  bytes[0]=seed;
  return bytes[0];
}

I64 AddressNativeS1(U8 *p,I64 i)
{
  return p[i];
}

I64 AddressNativeS2(I16 *p,I64 i)
{
  return p[i];
}

I64 AddressNativeS4(I32 *p,I64 i)
{
  return p[i];
}

I64 AddressNativeS8(I64 *p,I64 i)
{
  return p[i];
}

class AddressNativeRecord
{
  U32 pad;
  I32 value;
};

I64 AddressNativeMember(AddressNativeRecord *p,I64 i)
{
  return p[i].value;
}

I64 AddressNativeXor(I64 n)
{
  return n^0x12345678;
}

// Only inspect allocations belonging to the functions defined above.
I64 AddressNativeCodeSize(U8 *name,U8 *entry)
{
  CHashFun *fn=HashFind(name,Fs->hash_table,HTT_FUN);
  I64 size;
  if (!fn || !entry || fn->exe_addr!=entry)
    return 0;
  size=MSize(entry);
  if (size<=0 || size>512)
    return 0;
  return size;
}

I64 AddressNativeR9Saves(U8 *name,U8 *entry)
{
  I64 size=AddressNativeCodeSize(name,entry),i,count=0;
  for (i=0;i+3<=size;i++)
    if (entry[i]==0x49 && entry[i+1]==0x89 && entry[i+2]==0xC1)
      count++;
  return count;
}

Bool AddressNativeR9Spill(U8 *name,U8 *entry)
{
  I64 size=AddressNativeCodeSize(name,entry),i,saves=0,restores=0;
  for (i=0;i+2<=size;i++) {
    if (entry[i]==0x41 && entry[i+1]==0x51) saves++;
    if (entry[i]==0x41 && entry[i+1]==0x59) restores++;
  }
  return saves>0 && saves==restores;
}

// The condition lowering emits TEST RAX,RAX followed by JZ/JNZ.
Bool AddressNativeBranch(U8 *name,U8 *entry,Bool short_form)
{
  I64 size=AddressNativeCodeSize(name,entry),i,target;
  for (i=0;i+5<=size;i++) {
    if (entry[i]==0x48 && entry[i+1]==0x85 && entry[i+2]==0xC0) {
      if (short_form && (entry[i+3]==0x74 || entry[i+3]==0x75)) {
        target=i+5+entry[i+4](I8);
        if (target>=0 && target<size) return TRUE;
      } else if (!short_form && i+9<=size && entry[i+3]==0x0F &&
          (entry[i+4]==0x84 || entry[i+4]==0x85)) {
        target=i+9+*(entry+i+5)(I32 *);
        if (target>=0 && target<size && target-(i+9)>127) return TRUE;
      }
    }
  }
  return FALSE;
}

Bool AddressNativeBackJump()
{
  U8 *entry=&BranchLoop;
  I64 size=AddressNativeCodeSize("BranchLoop",entry),i,target;
  for (i=0;i+2<=size;i++)
    if (entry[i]==0xEB && entry[i+1](I8)<0) {
      target=i+2+entry[i+1](I8);
      if (target>=0 && target<i) return TRUE;
    }
  return FALSE;
}

// mode=1/2 tests [RBP+disp8/32]; mode=-1 tests a real SIB index.
// The optional displacement is checked exactly, and the scan is bounded.
Bool AddressNativeMem(U8 *name,U8 *entry,I64 opcode,I64 second,
    I64 wide,I64 mode,I64 scale,I64 disp)
{
  I64 size=AddressNativeCodeSize(name,entry),i,j,rex,mod,rm,sib,d,n;
  for (i=0;i<size;i++) {
    j=i;
    rex=0;
    if ((entry[j]&0xF0)==0x40) {
      rex=entry[j++];
      if (j>=size) goto skip_byte;
    }
    if (wide && !(rex&8)) goto skip_byte;
    if (entry[j++]!=opcode || j>=size) goto skip_byte;
    if (second>=0) {
      if (entry[j++]!=second || j>=size) goto skip_byte;
    }
    mod=entry[j]>>6;
    rm=entry[j++]&7;
    if (mod==3) goto skip_byte;
    if (mode<0) {
      if (rm!=4 || j>=size) goto skip_byte;
      sib=entry[j++];
      if ((1<<(sib>>6))!=scale ||
          (((sib>>3)&7)==4 && !(rex&2))) goto skip_byte;
      rm=sib&7;
    } else if (mod!=mode || rm!=5 || rex&1)
      goto skip_byte;
    n=0;
    if (mod==1) n=1;
    else if (mod==2 || (!mod && rm==5)) n=4;
    if (n>size-j) goto skip_byte;
    d=0;
    if (n==1) d=entry[j](I8);
    else if (n==4) d=*(entry+j)(I32 *);
    if (d==disp) return TRUE;
skip_byte:
    ;
  }
  return FALSE;
}

Bool AddressNativeImmediate()
{
  U8 *entry=&AddressNativeXor;
  I64 size=AddressNativeCodeSize("AddressNativeXor",entry),i;
  for (i=0;i+6<=size;i++)
    if (entry[i]==0x48) {
      // XOR RAX,imm32 or XOR r/m64,imm32 (register destination).
      if (entry[i+1]==0x35 &&
          *(entry+i+2)(U32 *)==0x12345678) return TRUE;
      if (i+7<=size && entry[i+1]==0x81 &&
          (entry[i+2]&0xF8)==0xF0 &&
          *(entry+i+3)(U32 *)==0x12345678) return TRUE;
    }
  return FALSE;
}

I64 address_native_values[2],*address_native_ptr,address_native_order;

I64 AddressNativeIndex()
{
  address_native_order=address_native_order*10+1;
  return 0;
}

I64 AddressNativeRhs()
{
  address_native_order=address_native_order*10+2;
  address_native_ptr=&address_native_values[1];
  address_native_values[0]=30;
  return 3;
}

U0 AddressFoldNativeProbe()
{
  I64 res=0,alias;
  CHashFun *fn;
  if (AddressNativeMem("AddressNativeFrame",&AddressNativeFrame,0x8B,-1,
          TRUE,1,0,-8) &&
      AddressNativeMem("AddressNativeFrame",&AddressNativeFrame,0x89,-1,
          TRUE,1,0,-8) && AddressNativeFrame(9)==16) res|=1;
  if (AddressNativeMem("AddressNativeFar",&AddressNativeFar,0x0F,0xB6,
          FALSE,2,0,-160) &&
      AddressNativeMem("AddressNativeFar",&AddressNativeFar,0x88,-1,
          FALSE,2,0,-160) && AddressNativeFar(0xE3)==0xE3) res|=2;
  if (AddressNativeMem("AddressNativeS1",&AddressNativeS1,0x0F,0xB6,
          FALSE,-1,1,0)) res|=4;
  if (AddressNativeMem("AddressNativeS2",&AddressNativeS2,0x0F,0xBF,
          TRUE,-1,2,0)) res|=8;
  if (AddressNativeMem("AddressNativeS4",&AddressNativeS4,0x63,-1,
          TRUE,-1,4,0)) res|=16;
  if (AddressNativeMem("AddressNativeS8",&AddressNativeS8,0x8B,-1,
          TRUE,-1,8,0)) res|=32;
  if (AddressNativeMem("AddressNativeMember",&AddressNativeMember,0x63,-1,
          TRUE,-1,8,4)) res|=64;
  if (AddressNativeImmediate() &&
      AddressNativeXor(7)==(7^0x12345678)) res|=128;

  // The destination address is captured before the RHS changes its base.
  address_native_values[0]=8;
  address_native_values[1]=13;
  address_native_ptr=&address_native_values[0];
  address_native_order=0;
  alias=address_native_ptr[AddressNativeIndex()]+=AddressNativeRhs();
  if (alias==33 && address_native_values[0]==33 &&
      address_native_values[1]==13 && address_native_order==12 &&
      address_native_ptr==&address_native_values[1]) res|=256;
  fn=HashFind("StackTempRead",Fs->hash_table,HTT_FUN);
  if (fn && fn->used_reg_mask&0x200 && fn->clobbered_reg_mask&0x200 &&
      AddressNativeR9Saves("StackTempRead",&StackTempRead)==1 &&
      AddressNativeR9Saves("StackTempNested",&StackTempNested)==1 &&
      AddressNativeR9Saves("StackTempDiv",&StackTempDiv)==1 &&
      AddressNativeR9Saves("StackTempShift",&StackTempShift)==1) res|=0x200;
  fn=HashFind("StackTempCall",Fs->hash_table,HTT_FUN);
  if (fn && fn->used_reg_mask&0x200 && fn->clobbered_reg_mask&0x200 &&
      AddressNativeR9Saves("StackTempCall",&StackTempCall)==1 &&
      AddressNativeR9Spill("StackTempCall",&StackTempCall) &&
      StackTempCall(13,7)==20) res|=0x400;
  if (AddressNativeBranch("BranchNear",&BranchNear,TRUE) &&
      AddressNativeBranch("BranchFar",&BranchFar,FALSE) &&
      AddressNativeBranch("BranchLoop",&BranchLoop,TRUE) &&
      AddressNativeBackJump()) res|=0x800;
  "Address fold native %X\n",res;
}

AddressFoldNativeProbe;
