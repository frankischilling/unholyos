#pragma once

#include "../../Kernel/KernelA.hpp"

enum : uint8_t
{
  SPT_END,
  SPT_COLOR,
  SPT_DITHER_COLOR,
  SPT_THICK,
  SPT_PLANAR_SYMMETRY,
  SPT_TRANSFORM_ON,
  SPT_TRANSFORM_OFF,
  SPT_SHIFT,
  SPT_PT,
  SPT_POLYPT,
  SPT_LINE,
  SPT_POLYLINE,
  SPT_RECT,
  SPT_ROTATED_RECT,
  SPT_CIRCLE,
  SPT_ELLIPSE,
  SPT_POLYGON,
  SPT_BSPLINE2,
  SPT_BSPLINE2_CLOSED,
  SPT_BSPLINE3,
  SPT_BSPLINE3_CLOSED,
  SPT_FLOOD_FILL,
  SPT_FLOOD_FILL_NOT,
  SPT_BITMAP,
  SPT_MESH,
  SPT_SHIFTABLE_MESH,
  SPT_ARROW,
  SPT_TEXT,
  SPT_TEXT_BOX,
  SPT_TEXT_DIAMOND,
  SPT_TYPES_NUM
};

constexpr uint8_t SPG_TYPE_MASK = 0x7F;

struct [[gnu::packed]] CMeshTri
{
  int32_t color, nums[3];
};

struct [[gnu::packed]] CSpriteBase
{
  uint8_t type;
};

struct [[gnu::packed]] CSpriteColor
{
  uint8_t type, color;
};

struct [[gnu::packed]] CSpriteDitherColor
{
  uint8_t type;
  uint16_t dither_color;
};

struct [[gnu::packed]] CSpriteT
{
  uint8_t type;
  int32_t thick;
};

struct [[gnu::packed]] CSpritePt
{
  uint8_t type;
  int32_t x1, y1;
};

struct [[gnu::packed]] CSpritePtRad
{
  CSpritePt p;
  int32_t radius;
};

struct [[gnu::packed]] CSpritePtPt
{
  CSpritePt p;
  int32_t x2, y2;
};

struct [[gnu::packed]] CSpritePtPtAng
{
  CSpritePtPt pp;
  double angle;
};

struct [[gnu::packed]] CSpritePtWH
{
  CSpritePt p;
  int32_t width, height;
};

struct [[gnu::packed]] CSpritePtWHAng
{
  CSpritePtWH pwh;
  double angle;
};

struct [[gnu::packed]] CSpritePtWHAngSides
{
  CSpritePtWHAng pwha;
  int32_t sides;
};

// Variable-length data follows these headers directly in the sprite stream.
struct [[gnu::packed]] CSpriteNumU8s
{
  uint8_t type;
  int32_t num;
};

struct [[gnu::packed]] CSpriteNumPtU8s
{
  uint8_t type;
  int32_t num, x, y;
};

struct [[gnu::packed]] CSpriteMeshU8s
{
  uint8_t type;
  int32_t vertex_cnt, tri_cnt;
};

struct [[gnu::packed]] CSpritePtMeshU8s
{
  uint8_t type;
  int32_t x, y, z, vertex_cnt, tri_cnt;
};

static_assert(sizeof(CMeshTri) == 16 && sizeof(CSpriteBase) == 1 &&
              sizeof(CSpriteColor) == 2 && sizeof(CSpriteDitherColor) == 3 &&
              sizeof(CSpriteT) == 5 && sizeof(CSpritePt) == 9 &&
              offsetof(CSpritePt, x1) == 1 && sizeof(CSpritePtRad) == 13 &&
              sizeof(CSpritePtPt) == 17 && sizeof(CSpritePtPtAng) == 25);
static_assert(sizeof(CSpritePtWH) == 17 && offsetof(CSpritePtWH, width) == 9 &&
              offsetof(CSpritePtWH, height) == 13 &&
              sizeof(CSpritePtWHAng) == 25 &&
              sizeof(CSpritePtWHAngSides) == 29 && sizeof(CSpriteNumU8s) == 5 &&
              sizeof(CSpriteNumPtU8s) == 13 && sizeof(CSpriteMeshU8s) == 9 &&
              sizeof(CSpritePtMeshU8s) == 21);

size_t SpriteElemSize(const uint8_t *elem, size_t available);
size_t SpriteSize(const uint8_t *elems, size_t available,
                  uint32_t *type_mask = nullptr);
bool Sprite3(CDC &dc, int64_t x, int64_t y, const uint8_t *elems,
             size_t available, bool just_one_elem = false);

extern "C" int64_t SpriteSizeCpp(const uint8_t *elems);
extern "C" void Sprite3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                           const uint8_t *elems, uint8_t just_one_elem);
extern "C" CDocEntryBase *DocSpriteCpp(CDoc *doc, const uint8_t *elems,
                                       const char *fmt);
extern "C" CDocEntryBase *SpriteCpp(const uint8_t *elems, const char *fmt);
