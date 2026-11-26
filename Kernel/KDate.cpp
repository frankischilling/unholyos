#include "KernelB.hpp"

uint16_t mon_start_days1[12]{0,31,59,90,120,151,181,212,243,273,304,334};
uint16_t mon_start_days2[12]{0,31,60,91,121,152,182,213,244,274,305,335};

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

CDate Struct2Date(const CDateStruct &ds)
{
  if (!ds.mon || ds.mon > 12)
    return {};
  auto start = YearStartDate(ds.year);
  const auto *months = YearStartDate(int64_t(ds.year) + 1) - start == 365
                           ? mon_start_days1
                           : mon_start_days2;
  uint64_t ticks = ds.sec10000 + 100 * (ds.sec100 + 100 * (ds.sec +
                    60 * (ds.min + 60 * uint64_t(ds.hour))));
  return {uint32_t((ticks << 21) / 421875),
          int32_t(start + months[ds.mon - 1] + ds.day_of_mon - 1)};
}

int64_t DayOfWeek(int64_t date)
{
  return date >= 0 ? date % 7 : 6 - (6 - date) % 7;
}

void Date2Struct(CDateStruct &ds, CDate cdt)
{
  int64_t date = cdt.date;
  ds.day_of_week = DayOfWeek(date);
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

int64_t FirstDayOfMon(int64_t date)
{
  CDateStruct ds;
  Date2Struct(ds, {0, int32_t(date)});
  ds.day_of_mon = 1;
  return Struct2Date(ds).date;
}

int64_t LastDayOfMon(int64_t date)
{
  CDateStruct ds;
  Date2Struct(ds, {0, int32_t(date)});
  if (++ds.mon == 13) {
    // The pinned LastDayOfMon sets mon to zero when advancing the year.
    // Its leap table's preceding word is mon_start_days1[11]; the
    // common-year table's preceding word is zero in the pinned image.
    auto start = YearStartDate(++ds.year);
    auto days = YearStartDate(int64_t(ds.year) + 1) - start;
    return int32_t(start + (days == 365 ? 0 : mon_start_days1[11])) - int64_t{1};
  }
  ds.day_of_mon = 1;
  return int64_t(Struct2Date(ds).date) - 1;
}

int64_t FirstDayOfYear(int64_t date)
{
  CDateStruct ds;
  Date2Struct(ds, {0, int32_t(date)});
  ds.mon = ds.day_of_mon = 1;
  return Struct2Date(ds).date;
}

int64_t LastDayOfYear(int64_t date)
{
  CDateStruct ds;
  Date2Struct(ds, {0, int32_t(date)});
  ds.mon = ds.day_of_mon = 1;
  ds.year++;
  return int64_t(Struct2Date(ds).date) - 1;
}

extern "C" uint64_t Struct2DateCpp(const CDateStruct *ds)
{
  return ds ? DateBits(Struct2Date(*ds)) : 0;
}

extern "C" void Date2StructCpp(CDateStruct *ds, uint64_t bits)
{
  if (ds)
    Date2Struct(*ds, DateFromBits(bits));
}

extern "C" int64_t YearStartDateCpp(int64_t year) { return YearStartDate(year); }
extern "C" int64_t DayOfWeekCpp(int64_t day) { return DayOfWeek(day); }
extern "C" int64_t FirstDayOfMonCpp(int64_t day) { return FirstDayOfMon(day); }
extern "C" int64_t LastDayOfMonCpp(int64_t day) { return LastDayOfMon(day); }
extern "C" int64_t FirstDayOfYearCpp(int64_t day) { return FirstDayOfYear(day); }
extern "C" int64_t LastDayOfYearCpp(int64_t day) { return LastDayOfYear(day); }

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
  CDateStruct ds{};
  ds.year = year;
  ds.mon = month;
  ds.day_of_mon = day;
  ds.hour = hour;
  ds.min = minute;
  ds.sec = second;
  return DateBits(Struct2Date(ds)) - DateBits(local_time_offset);
}
