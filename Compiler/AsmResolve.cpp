#include "../Kernel/KernelB.hpp"
#include "CompilerA.hpp"

extern "C" void PrintErrCpp(const char *fmt,int64_t argc,const uint64_t *argv,
                            const uintptr_t *frame);

const CHashExport *AsmExternal(const CCmpCtrl &cc,const CAOT *aot,
                               const char *name)
{
  auto *entry=HashFind(name,cc.htc.hash_table_lst,HTT_EXPORT_SYS_SYM);
  if (!entry && !aot)
    entry=HashFind(name,CurrentTask()->hash_table,HTT_EXPORT_SYS_SYM);
  return entry && !(entry->type & (HTF_IMPORT | HTF_UNRESOLVED))
             ? reinterpret_cast<const CHashExport *>(entry)
             : nullptr;
}

bool AsmResolve(CCmpCtrl &cc,CAOT *aot,CCodeCtrl *coc,size_t origin,
                const CAsmUnresolvedRef &fix,uint8_t *dest,uintptr_t base)
{
  uint64_t value=fix.num.addend;
  int bases=0;
  bool c_address=fix.num.c_address;
  auto add=[&](const CAsmSymbol *symbol,int sign) {
    if (!symbol)
      return true;
    if (symbol->block) {
      value+=sign*(AsmOrigin(*symbol->block)+symbol->offset);
      bases+=sign;
      return true;
    }
    if (coc && !symbol->imported) {
      if (auto *label=coc->GoToLabelFind(symbol->name)) {
        size_t offset=coc->code[label->ic].data;
        value+=sign*(offset+(aot ? aot->size : 0));
        bases+=sign;
        return true;
      }
    }
    auto *external=symbol->scope ? nullptr : AsmExternal(cc,aot,symbol->name);
    if (!external)
      return false;
    value+=sign*external->val;
    if (aot && !(external->sym.hash.type & HTF_IMM))
      bases+=sign;
    else
      c_address=!c_address;
    return true;
  };
  if (aot && fix.num.plus && fix.num.plus->imported) {
    if (fix.num.minus || value || (!fix.relative && fix.width!=8))
      return false;
    constexpr uint8_t rel_types[]{IET_REL_I8,IET_REL_I16,IET_REL_I32};
    auto type=fix.relative ? rel_types[__builtin_ctz(fix.width)]
                           : uint8_t{IET_IMM_I64};
    return AOTRelAdd(*aot,origin+fix.offset,nullptr,fix.num.plus->name,type);
  }
  if (!add(fix.num.plus,1) || !add(fix.num.minus,-1))
    return false;
  if (fix.relative) {
    value-=origin+fix.offset+fix.width;
    // Only an absolute target needs the image base subtracted.
    bases=c_address ? -1 : 0;
  }
  if (!aot)
    value+=bases*base;
  else if (bases) {
    if (bases!=1 || fix.relative || fix.width<4 ||
        !AOTRelAdd(*aot,origin+fix.offset,nullptr,nullptr,IET_ABS_ADDR))
      return false;
  }
  if (fix.relative) {
    unsigned shift=64-8*fix.width;
    if (uint64_t(int64_t(value<<shift)>>shift)!=value) {
      auto *label=fix.num.plus ? fix.num.plus->name : "";
      const uint64_t args[]{fix.line_num,reinterpret_cast<uintptr_t>(label)};
      PrintErrCpp("Branch out of range at line:%04d %s\n",2,args,nullptr);
      return false;
    }
  }
  MemCpy(dest+fix.offset,&value,fix.width);
  return true;
}

bool AsmUnresolvedAdd(CCmpCtrl &cc,CHeapCtrl *hc,CAsmUnresolvedRef *&fixups,
                      const CAsmNum &n,size_t offset,unsigned width,
                      bool relative,unsigned tail)
{
  auto *fix=reinterpret_cast<CAsmUnresolvedRef *>(
      MAlloc(sizeof(CAsmUnresolvedRef),hc));
  if (!fix)
    return false;
  *fix={fixups,n,offset,size_t(cc.lex_include_stk->line_num),width,relative};
  fix->num.addend-=tail;
  fixups=fix;
  return true;
}
