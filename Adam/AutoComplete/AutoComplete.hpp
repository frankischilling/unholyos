#pragma once

#include <stdint.h>

struct CTask;

inline constexpr char AC_STANDBY[] = "::/Doc/StandBy.DD.Z";

bool ACWordChar(uint8_t ch);
extern "C" void ACInitCpp(const char *mask);
void ACFillIn(int64_t n);
void ACMan(int64_t n, CTask *parent);
