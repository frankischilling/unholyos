#include "KernelB.hpp"

constexpr unsigned IDT_ENTRIES = 256;
constexpr uint16_t IDTET_IRQ = 0xE, GDT_CS64_SELECTOR = 0x40;

extern "C" void IntNop();

static void IntEntrySet(unsigned irq, void (*handler)(), uint16_t type,
                        uint16_t dpl)
{
  uintptr_t addr = reinterpret_cast<uintptr_t>(handler);
  uint64_t flags;
  asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
  auto &entry = dev.idt[irq];
  entry.offset_lo = addr;
  entry.selector = GDT_CS64_SELECTOR;
  entry.attributes = 0x8000 | (type << 8) | (dpl << 13);
  entry.offset_mid = addr >> 16;
  entry.offset_hi = addr >> 32;
  entry.reserved = 0;
  asm volatile("pushq %0; popfq" : : "r"(flags) : "cc", "memory");
}

bool IntInit1()
{
  uint64_t cpu;
  asm volatile("movq %%gs:8,%0" : "=r"(cpu));
  constexpr uint16_t bytes = IDT_ENTRIES * sizeof(CIdtEntry);
  if (!cpu) {
    dev.idt =
        reinterpret_cast<CIdtEntry *>(MAlloc(bytes, adam_task->data_heap));
    if (!dev.idt)
      return false;
    for (unsigned i = 0; i < IDT_ENTRIES; i++)
      IntEntrySet(i, IntNop, IDTET_IRQ, 0);
  }

  CSysLimitBase idtr{bytes - 1, reinterpret_cast<uintptr_t>(dev.idt)};
  asm volatile("lidt %0" : : "m"(idtr) : "memory");
  return true;
}

extern "C" {
void IntDivZero();
void IntMPCrash();
void IntWake();
void IRQ_TIMER();
void INT_FAULT();
[[noreturn]] void SysHlt();
}

void IntsInit()
{
  OutU8(0x20, 0x11);
  OutU8(0xA0, 0x11);
  OutU8(0x21, 0x20);
  OutU8(0xA1, 0x28);
  OutU8(0x21, 0x04);
  OutU8(0xA1, 0x02);
  OutU8(0x21, 0x0D);
  OutU8(0xA1, 0x09);
  OutU8(0x21, 0xFA);
  OutU8(0xA1, 0xFF);
}

static uint8_t *IntFaultHndlrsNew()
{
  auto *base = MAlloc(IDT_ENTRIES * 7, adam_task->code_heap);
  if (!base)
    return nullptr;
  for (unsigned i = 0; i < IDT_ENTRIES; i++) {
    auto *dst = base + i * 7;
    dst[0] = 0x6A;
    dst[1] = i;
    dst[2] = 0xE9;
    uint32_t rel = reinterpret_cast<uintptr_t>(INT_FAULT) -
                   reinterpret_cast<uintptr_t>(dst + 7);
    MemCpy(dst + 3, &rel, sizeof(rel));
  }
  return base;
}

bool IntInit2()
{
  auto *faults = IntFaultHndlrsNew();
  if (!faults)
    return false;
  dbg.int_fault_code = faults;
  IntEntrySet(I_DIV_ZERO, IntDivZero, IDTET_IRQ, 0);
  for (unsigned i = 1; i < 0x20; i++)
    IntEntrySet(i, reinterpret_cast<void (*)()>(faults + 7 * i), IDTET_IRQ, 0);
  IntEntrySet(I_NMI, SysHlt, IDTET_IRQ, 0);
  IntEntrySet(I_TIMER, IRQ_TIMER, IDTET_IRQ, 0);
  IntEntrySet(I_MP_CRASH, IntMPCrash, IDTET_IRQ, 0);
  IntEntrySet(I_WAKE, IntWake, IDTET_IRQ, 0);
  IntEntrySet(I_DBG, reinterpret_cast<void (*)()>(faults + 7 * I_DBG),
              IDTET_IRQ, 0);
  return true;
}
