#include "../KernelA.hpp"

constexpr uint16_t COM1 = 0x3F8;

static void SerialPutChar(char ch)
{
  for (unsigned tries = 0; tries < 65536; tries++)
    if (InU8(COM1 + 5) & 0x20) {
      OutU8(COM1, static_cast<uint8_t>(ch));
      return;
    }
}

void SerialPutS(const char *text)
{
  while (*text)
    SerialPutChar(*text++);
}

void SerialPutHex(uint64_t value)
{
  constexpr char digits[] = "0123456789abcdef";
  SerialPutS("0x");
  for (int bit = 60; bit >= 0; bit -= 4)
    SerialPutChar(digits[(value >> bit) & 0xF]);
}
