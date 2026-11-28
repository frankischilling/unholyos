// Run the public reports through the current task's output document.
U0 DevInfoProbe()
{
  I64 checks=0;
  Bool old_raw=Raw(FALSE);
  CDoc *saved=Fs->put_doc,*doc=DocNew;
  U8 *report,*repeat;

  doc->flags|=DOCF_NO_CURSOR;
  Fs->put_doc=doc;
  PCIRep;
  report=DocSave(doc);
  if (StrMatch("PCI Busses:1",report)) checks|=1;
  if (StrMatch("00:00:0 0600",report) &&
      StrMatch("$$FG,2$$Intel Corporation $$FG,3$$82440LX/EX PCI & Memory$$FG$$",
               report)) checks|=2;
  if (StrMatch("00:01:1 0101",report) &&
      StrMatch("82371SB PIIX3 IDE Interface (Triton II)",report)) checks|=4;
  if (StrMatch("Unknown",report)) checks|=8;
  DocClear(doc,1);
  PCIRep;
  repeat=DocSave(doc);
  if (!StrCmp(report,repeat)) checks|=16;
  Free(repeat);
  Free(report);

  DocClear(doc,1);
  MemBIOSRep;
  report=DocSave(doc);
  if (StrMatch("000A0000-000BFFFF VGA",report) &&
      StrMatch("MN:LAPIC_BASE",report) &&
      StrMatch("00:00000000E0000000-00000000EFFFFFFF",report)) checks|=32;
  if (StrMatch("BIOS Memory Report 15:E801",report) &&
      StrMatch("01:0000000000000000-0000000000FFFFFF",report) &&
      StrMatch("01:0000000001000000-000000001FFDFFFF",report)) checks|=64;
  if (StrMatch("BIOS Memory Report 15:E820",report) &&
      StrMatch("01:0000000000100000-000000001FFDFFFF",report)) checks|=128;
  Free(report);

  DocClear(doc,1);
  MemPageRep;
  report=DocSave(doc);
  if (StrMatch("MAPPED",report) && StrMatch("2MEG",report) &&
      StrMatch("PML2",report) && StrMatch("2MEG  :00080800",report) &&
      StrMatch("PML3",report) && StrMatch("1GIG  :00000404",report) &&
      StrMatch("PML4",report) && StrMatch("512GIG:00000003",report))
    checks|=256;
  if (StrMatch("FIXED_AREA:0000100000",report) &&
      StrMatch("HEAP_BASE :",report) && StrMatch("HEAP_LIMIT:",report))
    checks|=512;
  Free(report);
  Fs->put_doc=saved;
  DocDel(doc);
  Raw(old_raw);
  "Device reports %X\n",checks;
}

DevInfoProbe;
