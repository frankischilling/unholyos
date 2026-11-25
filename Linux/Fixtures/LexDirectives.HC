#help_index "Lexer"\"/Probe"
#define LEX_BASE 40
#define LEX_PAIR LEX_BASE + \
  2 // comment after a continued line
#define LEX_URL "http://temple//src"
#define LEX_EMPTY
#ifdef LEX_EMPTY
  #ifdef if
  LEX_PAIR LEX_URL LEX_EMPTY
  #endif
#else
  WRONG_EMPTY
#endif
#ifndef LEX_ABSENT
  #include "/LexInput"
#endif
#ifdef LEX_ABSENT
  #if 1
    #define LEX_BAD ignored
  #endif
#else
  LEX_TOP
#endif
#ifjit
  JIT
#else
  AOT
#endif
#ifaot
  AOT2
#else
  JIT2
#endif
#define LEX_BASE 21
LEX_BASE LEX_PAIR
#include "/LexMissing"
#help_file "LexGuide"
TAIL
