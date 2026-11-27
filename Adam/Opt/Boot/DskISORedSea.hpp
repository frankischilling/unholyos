#pragma once

#include "../../../Kernel/KernelA.hpp"

constexpr size_t ISO_CATALOG_LBA=20, ISO_LOADER_LBA=21;
constexpr size_t ISO_DRV_OFFSET=(ISO_LOADER_LBA+1)*DVD_BLK_SIZE/BLK_SIZE;

struct CDvdExtent
{
  size_t lba,sectors,shift;
};

enum CDvdExtentResult
{
  DVD_EXTENT_OK,DVD_EXTENT_MARKER,DVD_EXTENT_RANGE
};

CDvdExtent DvdExtent(size_t clus,size_t size);
void RedSeaISO9660(uint8_t *disc,uint32_t iso_size,uint32_t root_clus);
bool RedSeaISO9660Write(CDrv &dv);
CDvdExtentResult RedSeaISO9660Stage1(uint8_t *loader,const CDvdExtent &extent);
