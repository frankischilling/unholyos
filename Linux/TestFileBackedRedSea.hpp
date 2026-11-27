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
