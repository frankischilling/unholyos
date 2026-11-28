#include "DolDoc.hpp"
#include "../../Compiler/CompilerA.hpp"

static bool TreeName(const CDocEntryBase &entry, const char *name)
{
  return (entry.de_flags & DOCEF_TREE) && entry.tag &&
         StrLen(entry.tag) >= 3 &&
         !StrCmpCpp(reinterpret_cast<const uint8_t *>(entry.tag + 3),
                    reinterpret_cast<const uint8_t *>(name));
}

static int64_t TreeIndent(const CDocEntryBase &entry)
{
  return reinterpret_cast<const CDocEntry &>(entry).attr;
}

extern "C" uint64_t DocTreeFindCpp(CDoc *doc, const char *path,
                                    CDocEntryBase **tree,
                                    CDocEntryBase **start,
                                    CDocEntryBase **end)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !path)
    return false;
  CDocLock lock(*doc);
  auto *head = &doc->head;
  if (tree)
    *tree = head;
  if (start)
    *start = head;
  if (end)
    *end = head;
  auto *heap = CurrentTask()->data_heap;
  auto *remaining = StrNew(path, heap);
  auto *part = reinterpret_cast<char *>(MAlloc(StrLen(path) + 1, heap));
  bool found = false;
  int64_t indent = 0, level = 0;
  auto *entry = head->next;
  while (remaining && part && *remaining && entry != head) {
    StrFirstRemCpp(remaining, reinterpret_cast<const uint8_t *>("/"), part);
    if (!*part)
      continue;
    while (entry != head) {
      if (entry->Type() == DOCT_INDENT)
        indent += TreeIndent(*entry);
      else if (indent == level && TreeName(*entry, part)) {
        if (*remaining)
          break;
        if (tree)
          *tree = entry;
        while (entry != head && entry->Type() != DOCT_INDENT)
          entry = entry->next;
        if (entry != head) {
          indent = TreeIndent(*entry);
          if (start)
            *start = entry;
          for (entry = entry->next; entry != head && indent > 0;
               entry = entry->next)
            if (entry->Type() == DOCT_INDENT &&
                (indent += TreeIndent(*entry)) <= 0) {
              if (end)
                *end = entry;
              found = true;
              break;
            }
        }
        goto done;
      }
      entry = entry->next;
    }
    level += 2;
  }
done:
  Free(remaining);
  Free(part);
  return found;
}

extern "C" uint64_t DocTreeFFindCpp(const char *name, const char *path)
{
  auto *doc = DocReadCpp(name, 0);
  bool found = DocTreeFindCpp(doc, path, nullptr, nullptr, nullptr);
  DocDelCpp(doc);
  return found;
}

extern "C" uint64_t DocTreeMakeCpp(CDoc *doc, const char *path)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !path)
    return false;
  CDocLock lock(*doc);
  auto *heap = CurrentTask()->data_heap;
  auto *remaining = StrNew(path, heap);
  auto *part = reinterpret_cast<char *>(MAlloc(StrLen(path) + 1, heap));
  auto *missing = StrNew(path, heap);
  if (!remaining || !part || !missing) {
    Free(remaining);
    Free(part);
    Free(missing);
    return false;
  }
  auto *head = &doc->head;
  auto *entry = head->next;
  int64_t indent = 0, parent = INT64_MIN, level = 0;
  bool created = true;
  doc->cur_entry = head;
  doc->cur_col = 0;
  while (*remaining && entry != head) {
    StrFirstRemCpp(remaining, reinterpret_cast<const uint8_t *>("/"), part);
    if (!*part)
      continue;
    while (entry != head) {
      if (entry->Type() == DOCT_INDENT) {
        indent += TreeIndent(*entry);
        if (indent == parent) {
          doc->cur_entry = entry;
          doc->cur_col = 0;
          goto insert;
        }
      } else if (indent == level && TreeName(*entry, part)) {
        auto *rest = StrNew(remaining, heap);
        if (!rest) {
          created = false;
          goto done;
        }
        Free(missing);
        missing = rest;
        parent = indent;
        if (!*remaining)
          created = false;
        else
          break;
      }
      entry = entry->next;
    }
    level += 2;
  }
insert:
  if (created) {
    // Stage the missing branch before inserting it into the live document.
    auto *stage = DocNewCpp(nullptr, doc->mem_task);
    if (!stage) {
      created = false;
      goto done;
    }
    while (*missing) {
      StrFirstRemCpp(missing, reinterpret_cast<const uint8_t *>("/"), part);
      if (!*part)
        continue;
      uint64_t arg = reinterpret_cast<uintptr_t>(part);
      if (!DocPrintCpp(stage, "$TR+C,\"%s\"$\n$ID,2$", 1, &arg)) {
        created = false;
        break;
      }
      auto *close = DocPrintCpp(stage, "$ID,-2$", 0, nullptr);
      if (!close) {
        created = false;
        break;
      }
      stage->cur_entry = close;
      stage->cur_col = 0;
    }
    // The branch syntax cannot own a binary attachment. Do not transfer
    // entries with references into the staging document's binary list.
    if (stage->bin_head.next != &stage->bin_head)
      created = false;
    if (created && stage->head.next != &stage->head) {
      auto *first = stage->head.next;
      auto *last = stage->head.last;
      auto *cursor = stage->cur_entry;
      auto *before = doc->cur_entry;
      first->last = before->last;
      before->last->next = first;
      last->next = before;
      before->last = last;
      stage->head.next = stage->head.last = &stage->head;
      doc->cur_entry = cursor;
      doc->cur_col = 0;
    }
    DocDelCpp(stage);
  }
done:
  Free(remaining);
  Free(part);
  Free(missing);
  return created;
}

static uint64_t DocTreeChange(CDoc *doc, const char *path, uint8_t write,
                              const char *fmt, int64_t argc,
                              const uint64_t *argv, bool append)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL || !path || !fmt)
    return false;
  auto *text = StrPrintJoin(nullptr, fmt, argc, argv);
  if (!text)
    return false;
  CDocLock lock(*doc);
  CDocEntryBase *start, *end;
  bool found = DocTreeFindCpp(doc, path, nullptr, &start, &end);
  bool ready = found || DocTreeMakeCpp(doc, path);
  if (ready) {
    if (found) {
      if (!append)
        DocCutCpp(doc, start->next, end->last);
      doc->cur_entry = append ? end : start->next;
      doc->cur_col = doc->cur_entry->min_col;
    }
    DocPutS(*doc, text, StrLen(text));
    if (write && DrvIsWritableCpp(doc->filename.name[0], false))
      DocWriteCpp(doc, false);
  }
  Free(text);
  return found;
}

extern "C" uint64_t DocTreeWriteCpp(CDoc *doc, const char *path, uint8_t write,
                                     const char *fmt, int64_t argc,
                                     const uint64_t *argv)
{
  return DocTreeChange(doc, path, write, fmt, argc, argv, false);
}

extern "C" uint64_t DocTreeAppendCpp(CDoc *doc, const char *path, uint8_t write,
                                      const char *fmt, int64_t argc,
                                      const uint64_t *argv)
{
  return DocTreeChange(doc, path, write, fmt, argc, argv, true);
}

extern "C" uint64_t DocTreeFWriteCpp(const char *name, const char *path,
                                      const char *fmt, int64_t argc,
                                      const uint64_t *argv)
{
  auto *doc = DocReadCpp(name, 0);
  auto found = DocTreeWriteCpp(doc, path, true, fmt, argc, argv);
  DocDelCpp(doc);
  return found;
}

extern "C" uint64_t DocTreeFAppendCpp(const char *name, const char *path,
                                       const char *fmt, int64_t argc,
                                       const uint64_t *argv)
{
  auto *doc = DocReadCpp(name, 0);
  auto found = DocTreeAppendCpp(doc, path, true, fmt, argc, argv);
  DocDelCpp(doc);
  return found;
}

struct CDocCompile
{
  CCmpCtrl *cc;
  uint64_t value;
  bool okay;
};

extern "C" void DocCompileCpp(CDocCompile *run)
{
  Lex(*run->cc);
  run->okay = ExeCmdLine(*run->cc, run->value);
}

extern "C" void DocCompileHC();

// Compile the real DolDoc entries, including inserted binary operands.
CExeResult ExeDoc(CDoc &doc, int64_t flags)
{
  auto *task = CurrentTask();
  if (!CmpAsmHashLoad(task->hash_table))
    return {};
  CDocLock lock(doc);
  CCmpCtrl cc;
  CLexFile file;
  LexSourceInit(cc, file, nullptr, flags, doc.filename.name, task->hash_table);
  if (task->last_cc != reinterpret_cast<CCmpCtrl *>(&task->next_cc))
    cc.opts = task->last_cc->opts;
  if (!LexAttachDoc(cc, doc, &file)) {
    LexSourceDel(cc);
    return {};
  }
  QueIns(cc, *task->last_cc);
  CDocCompile run{&cc, 0, false};
  if (!CallHolyC1Checked(reinterpret_cast<uintptr_t>(DocCompileHC),
                         reinterpret_cast<uintptr_t>(&run))) {
    run.okay = false;
    cc.jit_exception = task->except_ch;
  }
  QueRem(cc);
  LexSourceDel(cc);
  return {run.okay ? run.value : 0, cc.jit_exception, run.okay,
          cc.parse_error_count != 0};
}

static uint64_t DocResult(CExeResult result)
{
  constexpr int64_t brk = 0x6B61657242;
  if (result.exception && result.exception != COMPILER_ERROR &&
      result.exception != brk)
    RethrowHolyC();
  return result.value;
}

extern "C" uint64_t ExeDocCpp(CDoc *doc, int64_t flags)
{
  return doc && doc->doc_signature == DOC_SIGNATURE_VAL
             ? DocResult(ExeDoc(*doc, flags)) : 0;
}

CExeResult DocTreeExe(CDoc *doc, const char *path)
{
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return {};
  CDoc *copy = nullptr;
  {
    CDocLock lock(*doc);
    CDocEntryBase *tree, *end;
    if (DocTreeFindCpp(doc, path, &tree, nullptr, &end))
      copy = DocCopyCpp(doc, tree, end);
  }
  if (!copy)
    return {};
  auto result = ExeDoc(*copy, 0);
  DocDelCpp(copy);
  return result;
}

extern "C" uint64_t DocTreeExeCpp(CDoc *doc, const char *path)
{
  return DocResult(DocTreeExe(doc, path));
}

extern "C" uint64_t DocTreeFExeCpp(const char *name, const char *path)
{
  auto *doc = DocReadCpp(name, 0);
  auto result = DocTreeExe(doc, path);
  DocDelCpp(doc);
  return DocResult(result);
}
