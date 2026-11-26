#include "../../Kernel/KernelB.hpp"
#include "Gr.hpp"

static constexpr uint8_t sprite_elem_base_sizes[SPT_TYPES_NUM]{
    sizeof(CSpriteBase),         // SPT_END
    sizeof(CSpriteColor),        // SPT_COLOR
    sizeof(CSpriteDitherColor),  // SPT_DITHER_COLOR
    sizeof(CSpriteT),            // SPT_THICK
    sizeof(CSpritePtPt),         // SPT_PLANAR_SYMMETRY
    sizeof(CSpriteBase),         // SPT_TRANSFORM_ON
    sizeof(CSpriteBase),         // SPT_TRANSFORM_OFF
    sizeof(CSpritePt),           // SPT_SHIFT
    sizeof(CSpritePt),           // SPT_PT
    sizeof(CSpriteNumPtU8s),     // SPT_POLYPT
    sizeof(CSpritePtPt),         // SPT_LINE
    sizeof(CSpriteNumU8s),       // SPT_POLYLINE
    sizeof(CSpritePtPt),         // SPT_RECT
    sizeof(CSpritePtPtAng),      // SPT_ROTATED_RECT
    sizeof(CSpritePtRad),        // SPT_CIRCLE
    sizeof(CSpritePtWHAng),      // SPT_ELLIPSE
    sizeof(CSpritePtWHAngSides), // SPT_POLYGON
    sizeof(CSpriteNumU8s),       // SPT_BSPLINE2
    sizeof(CSpriteNumU8s),       // SPT_BSPLINE2_CLOSED
    sizeof(CSpriteNumU8s),       // SPT_BSPLINE3
    sizeof(CSpriteNumU8s),       // SPT_BSPLINE3_CLOSED
    sizeof(CSpritePt),           // SPT_FLOOD_FILL
    sizeof(CSpritePt),           // SPT_FLOOD_FILL_NOT
    sizeof(CSpritePtWH),         // SPT_BITMAP
    sizeof(CSpriteMeshU8s),      // SPT_MESH
    sizeof(CSpritePtMeshU8s),    // SPT_SHIFTABLE_MESH
    sizeof(CSpritePtPt),         // SPT_ARROW
    sizeof(CSpritePt),           // SPT_TEXT
    sizeof(CSpritePt),           // SPT_TEXT_BOX
    sizeof(CSpritePt),           // SPT_TEXT_DIAMOND
};

size_t SpriteElemSize(const uint8_t *elem, size_t available)
{
  if (!elem || !available)
    return 0;
  auto type = *elem & SPG_TYPE_MASK;
  if (type >= SPT_TYPES_NUM)
    return 0;
  size_t base = sprite_elem_base_sizes[type], extra = 0;
  if (base > available)
    return 0;
  auto number = [elem](size_t offset) {
    int32_t res;
    MemCpy(&res, elem + offset, sizeof(res));
    return int64_t(res);
  };
  switch (type) {
  case SPT_POLYLINE:
  case SPT_POLYPT:
  case SPT_BSPLINE2:
  case SPT_BSPLINE2_CLOSED:
  case SPT_BSPLINE3:
  case SPT_BSPLINE3_CLOSED: {
    auto count = number(1);
    if (count < 0)
      return 0;
    extra = type == SPT_POLYPT     ? (count * 3 + 7) / 8
            : type == SPT_POLYLINE ? count * 8
                                   : count * sizeof(CD3I32);
    break;
  }
  case SPT_TEXT:
  case SPT_TEXT_BOX:
  case SPT_TEXT_DIAMOND:
    while (extra < available - base)
      if (!elem[base + extra++])
        return base + extra;
    return 0;
  case SPT_BITMAP: {
    int64_t w = number(offsetof(CSpritePtWH, width)),
            h = number(offsetof(CSpritePtWH, height));
    if (w < 0 || h < 0 || w > INT32_MAX - 7)
      return 0;
    extra = ((w + 7) & ~7) * h;
    break;
  }
  case SPT_MESH:
  case SPT_SHIFTABLE_MESH: {
    size_t offset = type == SPT_MESH ? offsetof(CSpriteMeshU8s, vertex_cnt)
                                     : offsetof(CSpritePtMeshU8s, vertex_cnt);
    auto vertices = number(offset), triangles = number(offset + 4);
    if (vertices < 0 || triangles < 0)
      return 0;
    extra = vertices * sizeof(CD3I32) + triangles * sizeof(CMeshTri);
    break;
  }
  }
  return extra <= available - base ? base + extra : 0;
}

size_t SpriteSize(const uint8_t *elems, size_t available, uint32_t *type_mask)
{
  uint32_t mask = 0;
  for (size_t pos = 0; elems && pos < available;) {
    auto type = elems[pos] & SPG_TYPE_MASK;
    auto size = SpriteElemSize(elems + pos, available - pos);
    if (!size)
      return 0;
    pos += size;
    if (type == SPT_END) {
      if (type_mask)
        *type_mask = mask;
      return pos;
    }
    mask |= uint32_t{1} << type;
  }
  return 0;
}

extern "C" int64_t SpriteSizeCpp(const uint8_t *elems)
{
  return SpriteSize(elems, INT64_MAX);
}
