#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

using Bytes = std::vector<uint8_t>;

inline Bytes FileRead(const std::filesystem::path &path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  auto size = file.tellg();
  if (size < 0)
    throw std::runtime_error("Cannot open " + path.string());

  Bytes bytes(static_cast<size_t>(size));
  file.seekg(0);
  if (!bytes.empty() &&
      !file.read(reinterpret_cast<char *>(bytes.data()), size))
    throw std::runtime_error("Cannot read " + path.string());
  return bytes;
}

inline bool HasBytes(std::span<const uint8_t> bytes, size_t offset,
                     std::string_view signature)
{
  if (offset > bytes.size() || signature.size() > bytes.size() - offset)
    return false;
  return std::string_view(reinterpret_cast<const char *>(bytes.data() + offset),
                          signature.size()) == signature;
}

inline uint64_t ReadLE(std::span<const uint8_t> bytes, size_t offset,
                       size_t width)
{
  if (offset > bytes.size() || width > bytes.size() - offset)
    throw std::runtime_error("Truncated boot record");

  uint64_t value = 0;
  for (size_t i = 0; i < width; i++)
    value |= uint64_t{bytes[offset + i]} << (i * 8);
  return value;
}

inline size_t CeilDiv(size_t value, size_t divisor)
{
  return value / divisor + (value % divisor != 0);
}
