#include "CompilerA.hpp"

bool OptBranch(CCodeCtrl &coc, uint32_t value, CICode branch, uint32_t target,
               unsigned depth)
{
  if (depth == 512 || coc.failed)
    return false;
  // Adding a label or branch can move the node array.
  const auto node = coc.code[value];
  auto opposite = branch == IC_BR_ZERO ? IC_BR_NOT_ZERO : IC_BR_ZERO;
  if (node.code == IC_NOT)
    return OptBranch(coc, node.left, opposite, target, depth + 1);
  if (node.code == IC_AND_AND || node.code == IC_OR_OR) {
    if ((node.code == IC_AND_AND) == (branch == IC_BR_ZERO))
      return OptBranch(coc, node.left, branch, target, depth + 1) &&
             OptBranch(coc, node.right, branch, target, depth + 1);
    auto done = coc.LabelNew();
    if (!OptBranch(coc, node.left, opposite, done, depth + 1) ||
        !OptBranch(coc, node.right, branch, target, depth + 1))
      return false;
    coc.Append(done);
    return true;
  }
  coc.Append(branch, value, target);
  return !coc.failed;
}
