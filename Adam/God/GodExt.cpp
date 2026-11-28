#include "GodExt.hpp"

CGodGlbls god;

bool GodExtInit()
{
  auto *doodle_fifo = FifoU8New(2048 * 8, adam_task);
  auto *fifo = FifoU8New(2048 * 8, adam_task);
  if (!doodle_fifo || !fifo) {
    FifoU8Del(doodle_fifo);
    FifoU8Del(fifo);
    return false;
  }
  god.doodle_fifo = doodle_fifo;
  god.fifo = fifo;
  return true;
}
