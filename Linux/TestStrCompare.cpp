#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

// StrA.asm also contains raw output entries. These test-only stubs satisfy
// their relocations; this executable calls only the comparison and uppercase
// entries.
extern "C" void PutHexHC()
{
}
extern "C" void PutCharsHC()
{
}
extern "C" void PutSHC()
{
}

struct Result
{
  int64_t value;
  uint64_t rbx, after, before;
};

static_assert(sizeof(Result) == 32);
static_assert(offsetof(Result, value) == 0);
static_assert(offsetof(Result, rbx) == 8);
static_assert(offsetof(Result, after) == 16);
static_assert(offsetof(Result, before) == 24);

extern "C" void CheckCompare(Result *, uint64_t, const char *, const char *,
                             int64_t);
extern "C" uint64_t CheckUpper(uint64_t);

asm(R"(
.intel_syntax noprefix
.text
.global CheckCompare
.type CheckCompare,@function
CheckCompare:
  PUSH RBP
  MOV RBP,RSP
  PUSH RBX
  PUSH R12
  PUSH R13
  PUSH R14
  PUSH R15
  SUB RSP,8
  MOV R12,RDI
  MOV R13,RSI
  MOV RDI,RDX
  MOV RSI,RCX
  MOV RDX,R8
  MOV RBX,0x1122334455667788
  MOV R14,RSP
  TEST R13,R13
  JZ .LCheckICmp
  CMP R13,1
  JE .LCheckNCmp
  CALL StrNICmpCpp
  JMP .LCheckDone
.LCheckNCmp:
  CALL StrNCmpCpp
  JMP .LCheckDone
.LCheckICmp:
  CALL StrICmpCpp
.LCheckDone:
  MOV [R12],RAX
  MOV [R12+8],RBX
  MOV [R12+16],RSP
  MOV [R12+24],R14
  MOV RSP,R14
  ADD RSP,8
  POP R15
  POP R14
  POP R13
  POP R12
  POP RBX
  POP RBP
  RET
.size CheckCompare,.-CheckCompare
.global CheckUpper
.type CheckUpper,@function
CheckUpper:
  SUB RSP,8
  MOV RAX,RDI
  CALL TO_UPPER
  ADD RSP,8
  RET
.size CheckUpper,.-CheckUpper
.att_syntax prefix
)");

int main()
{
  struct Case
  {
    const char *a, *b;
    int64_t n, i, c, ni;
  };
  const Case cases[]{
      {"", "", 0, 0, 0, 0},
      {"", "a", 3, -1, -1, -1},
      {"a", "", 3, 1, 1, 1},
      {"TempleOS", "tEMPLEos", 8, 0, -1, 0},
      {"a", "B", 1, -1, 1, -1},
      {"[", "`", 1, -1, -1, -1},
      {"Z", "[", 1, -1, -1, -1},
      {"`", "a", 1, 1, -1, 1},
      {"abc", "aBc", 3, 0, 1, 0},
      {"abc", "abd", 2, -1, 0, 0},
      {"abc", "abd", 3, -1, -1, -1},
      {"ab", "abc", 3, -1, -1, -1},
      {"Same", "sAME", -1, 0, -1, 0},
      {"abc", "ABD", -1, -1, 1, -1},
      {"abc", "ABC", INT64_MIN, 0, 1, 0},
      {"ab\0x", "ab\0y", 9, 0, 0, 0},
      {"aab", "ab", 3, -1, -1, -1},
      {"\x80", "\xFF", 1, -1, -1, -1},
      {"\xE0", "\xC0", -1, 1, 1, 1},
  };
  uint64_t calls = 0, bad = 0;
  auto run = [&](uint64_t which, const char *a, const char *b, int64_t n) {
    Result r{};
    CheckCompare(&r, which, a, b, n);
    calls++;
    if (r.rbx != 0x1122334455667788 || r.after != r.before) {
      std::printf("ABI failure: selector=%" PRIu64 " rbx=%016" PRIX64
                  " stack=%" PRId64 "\n",
                  which, r.rbx, int64_t(r.after - r.before));
      bad++;
    }
    return r.value;
  };
  for (const auto &c : cases) {
    bad += run(0, c.a, c.b, c.n) != c.i;
    bad += run(1, c.a, c.b, c.n) != c.c;
    bad += run(2, c.a, c.b, c.n) != c.ni;
  }
  bad += run(1, nullptr, nullptr, 0) != 0;
  bad += run(2, nullptr, nullptr, 0) != 0;
  uint64_t hashes[3]{0xCBF29CE484222325, 0xCBF29CE484222325,
                     0xCBF29CE484222325};
  uint64_t upper_hash = 0xCBF29CE484222325;
  char a[2]{}, b[2]{};
  for (unsigned i = 0; i < 256; i++) {
    a[0] = i;
    for (unsigned j = 0; j < 256; j++) {
      b[0] = j;
      for (unsigned k = 0; k < 3; k++)
        hashes[k] = (hashes[k] ^ uint64_t(run(k, a, b, 1) + 1)) * 0x100000001B3;
    }
    uint64_t input = 0x123456789ABCDE00 | i;
    uint64_t expected = input - (i >= 'a' && i <= 'z' ? 0x20 : 0);
    uint64_t value = CheckUpper(input);
    bad += value != expected;
    upper_hash = (upper_hash ^ value) * 0x100000001B3;
  }
  // StringCompareProbe.HC produced these hashes in the reference guest.
  bad += hashes[0] != 0x562AF89B93749399;
  bad += hashes[1] != 0xE94E7B76B0C5E825;
  bad += hashes[2] != 0x562AF89B93749399;
  bad += upper_hash != 0xED25B86AE3E78265;
  std::printf("SC bytes %016" PRIX64 " %016" PRIX64 " %016" PRIX64 "\n",
              hashes[0], hashes[1], hashes[2]);
  std::printf("SC upper %016" PRIX64 "\n", upper_hash);
  std::printf("ABI calls=%" PRIu64 " upper=256 failures=%" PRIu64 "\n", calls,
              bad);
  return bad ? 1 : 0;
}
