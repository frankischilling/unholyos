#include "KernelB.hpp"

static uint8_t CMOSRead(uint8_t reg)
{
  OutU8(0x70, reg);
  return InU8(0x71);
}

static unsigned FromBCD(uint8_t n)
{
  return (n >> 4) * 10 + (n & 15);
}

int64_t YearStartDate(int64_t year)
{
  int64_t last = year - 1;
  return year * 365 + last / 4 - last / 100 + last / 400 - last / 4000;
}

void Date2Struct(CDateStruct &ds, CDate cdt)
{
  constexpr uint16_t mon_start_days1[]{0,   31,  59,  90,  120, 151,
                                       181, 212, 243, 273, 304, 334};
  constexpr uint16_t mon_start_days2[]{0,   31,  60,  91,  121, 152,
                                       182, 213, 244, 274, 305, 335};
  int64_t date = cdt.date;
  ds.day_of_week = date >= 0 ? date % 7 : 6 - (6 - date) % 7;
  ds.year = (date + 1) * 100000 / 36524225;
  auto start = YearStartDate(ds.year);
  while (start > date)
    start = YearStartDate(--ds.year);
  date -= start;
  const auto *months = YearStartDate(ds.year + 1) - start == 365
                           ? mon_start_days1
                           : mon_start_days2;
  uint8_t month = 0;
  while (month < 11 && date >= months[month + 1])
    month++;
  ds.mon = month + 1;
  ds.day_of_mon = date - months[month] + 1;
  uint64_t ticks = (421875 * uint64_t(cdt.time) >> 21) + 1;
  ds.sec10000 = ticks % 100;
  ticks /= 100;
  ds.sec100 = ticks % 100;
  ticks /= 100;
  ds.sec = ticks % 60;
  ticks /= 60;
  ds.min = ticks % 60;
  ds.hour = ticks / 60;
}

extern "C" uint64_t NowCpp()
{
  return Now();
}

uint64_t Now()
{
  static uint64_t locked_flags;
  CMemLock lock(locked_flags);
  constexpr uint8_t fields[]{9, 8, 7, 4, 2, 0};
  uint8_t read[sizeof(fields)];
  do {
    while (CMOSRead(0x0A) & 0x80)
      asm volatile("pause");
    for (size_t i = 0; i < sizeof(fields); i++)
      read[i] = CMOSRead(fields[i]);
  } while ((CMOSRead(0x0A) & 0x80) || read[5] != CMOSRead(0));
  uint8_t mode = CMOSRead(0x0B);
  bool pm = read[3] & 0x80;
  read[3] &= 0x7F;
  unsigned value[sizeof(fields)];
  for (size_t i = 0; i < sizeof(fields); i++)
    value[i] = mode & 4 ? read[i] : FromBCD(read[i]);
  unsigned year = 2000 + value[0], month = value[1], day = value[2];
  unsigned hour = value[3], minute = value[4], second = value[5];
  if (!(mode & 2))
    hour = hour % 12 + (pm ? 12 : 0);
  if (!month || month > 12 || !day || day > 31 || hour > 23 || minute > 59 ||
      second > 59)
    return 0;
  constexpr unsigned months[]{0,   31,  59,  90,  120, 151,
                              181, 212, 243, 273, 304, 334};
  uint64_t days = YearStartDate(year);
  days += months[month - 1] + day - 1;
  if (month > 2 && YearStartDate(year + 1) - YearStartDate(year) == 366)
    days++;
  uint64_t secs = (hour * 60 + minute) * 60 + second;
  return (days << 32) | ((secs << 32) / 86400);
}
