#include "DolDoc/DolDoc.hpp"

extern "C" void PrintCpp(const char *fmt, int64_t argc, const uint64_t *argv);

constexpr char PCI_DEV_FILE[] = "::/Misc/PCIDevices.DD.Z";

template <class... T> static void RepPrint(const char *fmt, T... args)
{
  uint64_t values[]{uint64_t(args)..., 0};
  PrintCpp(fmt, sizeof...(args), values);
}

static bool PCIId(const CDocEntryBase &entry, uint16_t id)
{
  if (entry.Type() != DOCT_TEXT || StrLen(entry.tag) < 4)
    return false;
  char buf[7]{'0', 'x'};
  MemCpy(buf + 2, entry.tag, 4);
  return Str2I64Cpp(buf, 10, nullptr) == id;
}

static void PCILookUpSingle(CDoc &doc, uint16_t m, uint16_t d,
                            uint8_t *&_vendor, uint8_t *&_dev)
{
  auto *head = &doc.head;
  auto *doc_e = head->next;
  const char *vendor = nullptr, *name = nullptr;
  bool first = true;
  for (; doc_e != head; doc_e = doc_e->next) {
    if (first && PCIId(*doc_e, m) && doc_e->tag[0] != ';' &&
        doc_e->next != head && doc_e->next->next != head) {
      doc_e = doc_e->next->next;
      if (doc_e->Type() == DOCT_TEXT) {
        vendor = doc_e->tag;
        break;
      }
    }
    first = doc_e->Type() == DOCT_NEW_LINE;
  }

  if (vendor) {
    first = false;
    for (; doc_e != head; doc_e = doc_e->next) {
      if (first) {
        if (doc_e->Type() != DOCT_TAB)
          break;
        doc_e = doc_e->next;
        if (doc_e == head)
          break;
        if (PCIId(*doc_e, d) && doc_e->next != head &&
            doc_e->next->next != head) {
          doc_e = doc_e->next->next;
          if (doc_e->Type() == DOCT_TEXT) {
            name = doc_e->tag;
            break;
          }
        }
      }
      first = doc_e->Type() == DOCT_NEW_LINE;
    }
  }

  auto *heap = adam_task->data_heap;
  _vendor =
      reinterpret_cast<uint8_t *>(StrNew(vendor ? vendor : "Unknown", heap));
  _dev = reinterpret_cast<uint8_t *>(StrNew(name ? name : "Unknown", heap));
}

static void PCILookUpDevs()
{
  static uint64_t pci_lookup_lock;
  auto &head = dev.pci_head;
  {
    CMemLock lock(pci_lookup_lock);
    if (head.next != &head)
      return;
  }
  auto *doc = DocRead(PCI_DEV_FILE, DOCF_PLAIN_TEXT | DOCF_NO_CURSOR, nullptr);
  if (!doc)
    return;

  CPCIDev pending, *tmppci;
  QueInit(pending.next, pending.last);
  bool complete = true;
  constexpr int64_t search_limit = 32 * 8 * 2;
  int64_t timeout = search_limit;
  for (uint16_t b = 0; b < SYS_PCI_BUSES; b++)
    for (uint8_t d = 0; d < 32; d++)
      for (uint8_t f = 0; f < 8; f++) {
        uint16_t w1 = PCIReadU16(b, d, f, 0);
        if (w1 != 0xFFFF) {
          tmppci = reinterpret_cast<CPCIDev *>(
              CAlloc(sizeof(CPCIDev), adam_task->data_heap));
          if (!tmppci) {
            complete = false;
            goto lud_done;
          }
          tmppci->bus = b;
          tmppci->dev = d;
          tmppci->fun = f;
          tmppci->vendor = w1;
          uint16_t w2 = tmppci->dev_id = PCIReadU16(b, d, f, 2);
          tmppci->sub_code = PCIReadU8(b, d, f, 0xA);
          tmppci->base_code = PCIReadU8(b, d, f, 0xB);
          PCILookUpSingle(*doc, w1, w2, tmppci->vendor_str, tmppci->dev_id_str);
          QueIns(*tmppci, *pending.last);
          if (!tmppci->vendor_str || !tmppci->dev_id_str) {
            complete = false;
            goto lud_done;
          }
          timeout = search_limit;
        } else if (SYS_PCI_BUSES == 256 && --timeout <= 0)
          goto lud_done;
      }
lud_done:
  DocDelCpp(doc);
  if (complete && pending.next != &pending) {
    CMemLock lock(pci_lookup_lock);
    if (head.next == &head) {
      head.last = pending.last;
      head.next = pending.next;
      head.next->last = &head;
      head.last->next = &head;
      return;
    }
  }
  while (pending.next != &pending) {
    tmppci = pending.next;
    QueRem(*tmppci);
    Free(tmppci->vendor_str);
    Free(tmppci->dev_id_str);
    Free(tmppci);
  }
}

extern "C" void PCIRep()
{
  RepPrint("PCI Busses:%d\n", SYS_PCI_BUSES);
  if (!FileFindCpp(PCI_DEV_FILE, nullptr, 0)) {
    PutS("You don't have the PCI device file.\n");
    return;
  }
  PCILookUpDevs();
  auto &head = dev.pci_head;
  for (auto *tmppci = head.next; tmppci != &head; tmppci = tmppci->next)
    RepPrint("%02X:%02X:%01X %02X%02X $GREEN$%s $CYAN$%s$FG$\n", tmppci->bus,
             tmppci->dev, tmppci->fun, tmppci->base_code, tmppci->sub_code,
             tmppci->vendor_str, tmppci->dev_id_str);
}

extern "C" void MemBIOSRep()
{
  PutS("Standard Addresses\n"
       "000A0000-000BFFFF VGA\n"
       "FEE00000-FEE00FFF See $LK,\"APIC\",A=\"MN:LAPIC_BASE\"$\n\n"
       "32 Bit Device Mem\n");
  auto &flags = sys_semas[SEMA_DEV_MEM].flags;
  while (__atomic_fetch_or(&flags, 1, __ATOMIC_ACQUIRE) & 1)
    _YIELD();
  auto &head = dev.mem32_head;
  for (auto *tmpmr = head.next; tmpmr != &head; tmpmr = tmpmr->next)
    RepPrint("%02X:%016X-%016X\n", tmpmr->type, tmpmr->base,
             tmpmr->base + tmpmr->size - 1);
  __atomic_fetch_and(&flags, ~uint64_t{1}, __ATOMIC_RELEASE);

  RepPrint("\nBIOS Memory Report 15:E801\n"
           "01:0000000000000000-%016X\n",
           SYS_FIXED_AREA + (uint64_t(MEM_E801[0]) << 10) - 1);
  RepPrint("01:0000000001000000-%016X\n",
           SYS_16MEG_AREA_LIMIT + (uint64_t(MEM_E801[1]) << 16) - 1);

  if (MEM_E820[0].type) {
    PutS("\nBIOS Memory Report 15:E820\n");
    for (const auto &m20 : MEM_E820) {
      if (!m20.type)
        break;
      RepPrint("%02X:%016X-%016X\n", m20.type, m20.base,
               m20.base + m20.len - 1);
    }
  }
}

extern "C" void MemPageRep()
{
  RepPrint("MAPPED\t  :%010X with ", MEM_MAPPED_SPACE);
  if (MEM_PAGE_SIZE & (1 << 30))
    PutS("$RED$1GIG$FG$ pages\n");
  else
    PutS("$RED$2MEG$FG$ pages\n");
  RepPrint("PML2\t  :%010X 2MEG  :%08X\n", MEM_PML2, MEM_2MEG_NUM);
  RepPrint("PML3\t  :%010X 1GIG  :%08X\n", MEM_PML3, MEM_1GIG_NUM);
  RepPrint("PML4\t  :%010X 512GIG:%08X\n", MEM_PML4, MEM_512GIG_NUM);
  RepPrint("FIXED_AREA:%010X\n", SYS_FIXED_AREA);
  RepPrint("HEAP_BASE :%010X\nHEAP_LIMIT:%010X\n", MEM_HEAP_BASE,
           MEM_HEAP_LIMIT);
}
