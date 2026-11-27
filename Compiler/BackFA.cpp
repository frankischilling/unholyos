#include "CompilerB.hpp"

void FltBinary(CCodeOut &out, const CIntermediateCode &node,
                      CRawType left, CRawType right, unsigned stack_slots)
{
  auto op = node.code;
  out.Bytes(0x50, 0x51); // PUSH RAX / PUSH RCX
  out.ToFloat(left, 1);
  out.ToFloat(right, 0);
  if (op == IC_POWER) {
    // SYS_POW takes its binary64 operands in RDX and RAX. A named
    // TOSB relocation keeps the AOT module independent of load address.
    if (out.AOT())
      out.RelAddress(nullptr,"SYS_POW");
    else {
      auto address=CmpSysExport("SYS_POW");
      if (!address) {
        out.failed=true;
        return;
      }
      out.MoveRAX(address);
    }
    out.Bytes(0x48,0x89,0xC1,0x58,0x5A); // MOV RCX,RAX / POP RAX / POP RDX
    bool spilled=out.SpillR9();
    bool pad = !((stack_slots+spilled) & 1);
    if (pad)
      out.Bytes(0x48, 0x83, 0xEC, 8); // SUB RSP,8
    out.Bytes(0xFF,0xD1); // CALL RCX
    if (pad)
      out.Bytes(0x48, 0x83, 0xC4, 8); // ADD RSP,8
    out.RestoreR9(spilled);
    return;
  }
  if (op == IC_AND || op == IC_OR || op == IC_XOR || op == IC_SHL ||
      op == IC_SHR ||
      (!node.runtime && (op == IC_EQU_EQU || op == IC_NOT_EQU))) {
    out.Bytes(0x48, 0x8B, 0x44, 0x24, 8); // MOV RAX,[RSP+8]
    switch (op) {
    case IC_AND:
      out.Bytes(0x48, 0x23, 0x04, 0x24);
      break;
    case IC_OR:
      out.Bytes(0x48, 0x0B, 0x04, 0x24);
      break;
    case IC_XOR:
      out.Bytes(0x48, 0x33, 0x04, 0x24);
      break;
    case IC_SHL:
    case IC_SHR:
      out.Bytes(0x48, 0x8B, 0x0C, 0x24); // MOV RCX,[RSP]
      IntBinary(out, op, false);
      break;
    default:
      out.Bytes(0x48, 0x3B, 0x04, 0x24); // CMP RAX,[RSP]
      out.SetBool(CmpCondition(op, true));
      break;
    }
    out.DropPair();
    return;
  }
  if (ICIsComparison(op)) {
    out.FloatMem(0xDD, 0, 0);
    out.FloatMem(0xDD, 0, 1);
    out.Bytes(0xDF, 0xE9, 0xDD, 0xD8); // FUCOMIP ST0,ST1 / FSTP ST0
    out.DropPair();
    out.SetBool(CmpCondition(op, true));
    if (op == IC_LESS || op == IC_LESS_EQU || op == IC_EQU_EQU)
      out.Bytes(0x0F, 0x9B, 0xC2, 0x20, 0xD0); // SETNP DL / AND AL,DL
    else if (op == IC_NOT_EQU)
      out.Bytes(0x0F, 0x9A, 0xC2, 0x08, 0xD0); // SETP DL / OR AL,DL
    return;
  }
  if (op == IC_MOD) {
    out.Bytes(0x58,0x5A); // POP RAX / POP RDX
    ICCopyTemplate(out,CMP_TEMPLATE_MOD);
    return;
  } else {
    out.FloatMem(0xDD, 0, 1);
    uint8_t math = op == IC_ADD ? 0 : op == IC_MUL ? 1 : op == IC_SUB ? 4 : 6;
    out.FloatMem(0xDC, math, 0);
  }
  out.FloatMem(0xDD, 3, 1);
  out.Bytes(0x59, 0x58); // POP RCX / POP RAX
}
