#pragma once

constexpr std::string_view file_backed_redsea_exceptions[]{
    "File", "Drv", "BlkDev", "Drv"};

constexpr CTextProbe file_backed_redsea_probes[]{
    {"file-backed-redsea", "/FileBackedRedSeaProbe.HC", "",
     "File backed RedSea ", "File backed RedSea ",
     "File backed RedSea 1FF\n", {}, true},
    {"file-backed-redsea-write", "/FileBackedRedSeaWriteProbe.HC", "",
     "File backed RedSea write ", "File backed RedSea write ",
     "File backed RedSea write 7FF\n", {}, true},
    {"file-backed-redsea-native", "/FileBackedRedSeaNativeProbe.HC", "",
     "File backed RedSea native ", "File backed RedSea native ",
     "File backed RedSea native 7FF\n", file_backed_redsea_exceptions, true}};

constexpr std::string_view nested_redsea_exceptions[]{"BlkDev","Drv"};
constexpr CTextProbe nested_redsea_probes[]{
    {"nested-redsea", "/NestedRedSeaProbe.HC", "", "Nested RedSea ",
     "Nested RedSea ", "Nested RedSea 7F\n", {}, true},
    {"redsea-free-list", "/RedSeaFreeListProbe.HC", "", "RedSea free list ",
     "RedSea free list ", "RedSea free list 7F\n", {}, true},
    {"redsea-iso-image", "/RedSeaISOImageProbe.HC", "", "RedSea ISO image ",
     "RedSea ISO image ", "RedSea ISO image FF\n", {}, true},
    {"nested-redsea-native", "/NestedRedSeaNativeProbe.HC", "", "Nested RedSea native ",
     "Nested RedSea native ", "Nested RedSea native 7F\n", nested_redsea_exceptions, true}};

constexpr CTextProbe fat32_image_probes[]{
    {"fat32-image", "/Fat32ImageProbe.HC", "", "FAT32 image ",
     "FAT32 image ", "FAT32 image 7F\n", {}, true},
    {"fat32-image-reboot", "/Fat32ImageProbe.HC", "", "FAT32 image reboot ",
     "FAT32 image reboot ", "FAT32 image reboot 7\n", {}, true}};

constexpr std::string_view optical_image_exceptions[]{"Drv","Drv"};
constexpr CTextProbe optical_image_probes[]{
    {"optical-image", "/OpticalImageProbe.HC", "",
     "Optical image ", "Optical image ",
     "Optical image 3F\n", optical_image_exceptions, true}};
