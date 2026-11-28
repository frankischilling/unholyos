#include "CompilerA.hpp"

// The reference's OptPass789A repeats code layout until sizes stop
// shrinking. Here the emitter records offsets; this pass only selects
// additional short branches, without changing the indexed IR or offsets.
bool OptPass789A(CIntermediateCode *code,uint32_t count)
{
  for (uint32_t i=0;i<count;i++)
    switch (code[i].code) {
      case IC_ASM:
      case IC_ASM_ADDR:
      case IC_GET_LABEL:
        // Assembly can contain alignment and location-dependent fixups.
        // Do not relax branches in a block with opaque code locations.
        return false;
      default:
        break;
    }

  bool changed=false;
  for (uint32_t i=0;i<count;i++) {
    auto &node=code[i];
    uint32_t near_size;
    switch (node.code) {
      case IC_JMP: case IC_RETURN_VAL:
        near_size=5;
        break;
      case IC_BR_ZERO: case IC_BR_NOT_ZERO:
        near_size=6;
        break;
      default:
        continue;
    }
    if (node.short_branch || node.branch_end<near_size ||
        node.branch_end>INT32_MAX || node.right>=count)
      continue;
    const auto &label=code[node.right];
    if (label.code!=IC_LABEL || label.data>INT32_MAX)
      continue;

    const auto end=static_cast<int64_t>(node.branch_end);
    const auto target=static_cast<int64_t>(label.data);
    auto disp=target-end;
    if (target<end) {
      if (target>end-near_size)
        continue; // A label cannot be inside the branch instruction.
      // A backward target does not move when this branch shrinks.
      disp+=near_size-2;
    }
    // A forward target and the branch end both move by the same amount.
    // Shortening other branches between the two can only reduce distance.
    if (disp>=INT8_MIN && disp<=INT8_MAX) {
      node.short_branch=true;
      changed=true;
    }
  }
  return changed;
}
