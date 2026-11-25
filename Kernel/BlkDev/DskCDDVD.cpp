#include "../KernelB.hpp"

bool ISOInit(CDrv &dv)
{
  constexpr uint8_t ISOT_BOOT_RECORD = 0, ISOT_SUPPLEMENTARY_DESC = 2,
                    ISOT_TERMINATOR = 0xFF;
  constexpr unsigned sectors_per_dvd = DVD_BLK_SIZE / BLK_SIZE;
  constexpr char publisher[] = "TempleOS RedSea";
  uint64_t disc_sectors = (uint64_t(dv.bd->max_blk) + 1) / sectors_per_dvd;
  uint64_t drv_offset = 19 * sectors_per_dvd;
  uint8_t descriptor[DVD_BLK_SIZE];
  for (uint64_t lba = 16; lba < disc_sectors; lba++) {
    if (!ATAPIReadBlks2(*dv.bd, descriptor, lba, 1))
      return false;
    uint32_t id;
    MemCpy(&id, descriptor + 1, sizeof(id));
    if (id != 0x30304443 || descriptor[5] != '1' ||
        descriptor[0] == ISOT_TERMINATOR)
      return false;
    if (descriptor[0] == ISOT_BOOT_RECORD) {
      drv_offset += 3 * sectors_per_dvd;
    } else if (descriptor[0] == ISOT_SUPPLEMENTARY_DESC) {
      for (size_t i = 0; i < sizeof(publisher); i++)
        if (descriptor[314 + i] != publisher[i])
          return false;
      uint32_t sectors;
      MemCpy(&sectors, descriptor + 80, sizeof(sectors));
      uint64_t total = uint64_t{sectors} * sectors_per_dvd;
      if (sectors > disc_sectors || drv_offset >= total)
        return false;
      dv.bd->max_blk = total - 1;
      dv.bd->drv_offset = dv.drv_offset = drv_offset;
      return RedSeaInit(dv);
    }
  }
  return false;
}
