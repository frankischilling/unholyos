#pragma once

#include <stdint.h>

struct CTask;
struct CDoc;

inline constexpr char AC_STANDBY[] = "::/Doc/StandBy.DD.Z";
inline constexpr char ACD_WORD_FILENAME[] = "/Adam/AutoComplete/ACWords.DATA.Z";
inline constexpr char ACD_DEF_FILENAME[] = "/Adam/AutoComplete/ACDefs.DATA";
inline constexpr char ACD_DEF_FILENAME_Z[] = "/Adam/AutoComplete/ACDefs.DATA.Z";

bool ACWordChar(uint8_t ch);
extern "C" void ACInitCpp(const char *mask);
extern "C" void ACDWordsLoadCpp();
extern "C" uint8_t *ACDWordPtAtCpp(const char *word);
extern "C" char *ACDDefGetCpp(const char *word, int64_t def_num);
extern "C" uint8_t *ACDDefsGetCpp(const char *word);
extern "C" void ACDDefsPutCpp(CDoc *doc, const char *word, int64_t num);
uint8_t *ACDWordNext(uint8_t *record);
void ACDFillin(int64_t n);
void ACDDef(int64_t n, CTask *parent);
void ACFillIn(int64_t n);
void ACMan(int64_t n, CTask *parent);
