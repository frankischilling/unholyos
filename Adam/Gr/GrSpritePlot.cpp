#include "../DolDoc/DolDoc.hpp"
#include "Gr.hpp"
#include "GrExt.hpp"
#include "GrGlbls.hpp"

static void SpritePolyPt(CDC &dc, uint64_t x, uint64_t y, uint64_t z,
                         const CSpriteNumPtU8s &elem, const uint8_t *points)
{
  constexpr int8_t gr_x_offsets[]{-1, 0, 1, -1, 1, -1, 0, 1},
      gr_y_offsets[]{-1, -1, -1, 0, 0, 1, 1, 1};
  x += elem.x;
  y += elem.y;
  GrPlot3Cpp(&dc, x, y, z);
  for (int64_t i = 0; i < int64_t(elem.num) * 3; i += 3) {
    uint16_t bits = points[i / 8];
    if (i % 8 > 5)
      bits |= uint16_t(points[i / 8 + 1]) << 8;
    auto j = (bits >> (i % 8)) & 7;
    x += gr_x_offsets[j];
    y += gr_y_offsets[j];
    GrPlot3Cpp(&dc, x, y, z);
  }
}

static void SpritePolyline(CDC &dc, uint64_t x, uint64_t y, uint64_t z,
                           const uint8_t *points, int32_t count)
{
  int32_t p[2], previous[2];
  if (count < 2)
    return;
  MemCpy(previous, points, sizeof(previous));
  for (int32_t i = 1; i < count; i++) {
    points += sizeof(p);
    MemCpy(p, points, sizeof(p));
    GrLine3Cpp(&dc, previous[0] + x, previous[1] + y, z, p[0] + x, p[1] + y, z,
               1, 0);
    MemCpy(previous, p, sizeof(p));
  }
}

bool Sprite3(CDC &dc, int64_t x0, int64_t y0, int64_t z0, const uint8_t *elems,
             size_t available, bool just_one_elem)
{
  constexpr uint32_t supported =
      (1u << SPT_END) | (1u << SPT_COLOR) | (1u << SPT_DITHER_COLOR) |
      (1u << SPT_THICK) | (1u << SPT_SHIFT) | (1u << SPT_PT) |
      (1u << SPT_POLYPT) | (1u << SPT_LINE) | (1u << SPT_POLYLINE) |
      (1u << SPT_RECT) | (1u << SPT_BITMAP) | (1u << SPT_TRANSFORM_ON) |
      (1u << SPT_TRANSFORM_OFF) | (1u << SPT_PLANAR_SYMMETRY) |
      (1u << SPT_ARROW);
  constexpr uint32_t flags_3d = DCF_TRANSFORMATION | DCF_SYMMETRY;
  if (!elems || !available)
    return false;
  uint32_t mask = 0;
  size_t size;
  if (just_one_elem) {
    size = SpriteElemSize(elems, available);
    if (size)
      mask = 1u << (*elems & SPG_TYPE_MASK);
  } else {
    size = SpriteSize(elems, available, &mask);
  }
  if (!size || (mask & ~supported))
    return false;
  uint64_t x = x0, y = y0, z = z0;
  auto old_color = dc.color;
  auto old_flags = dc.flags;
  auto old_sym = dc.sym;
  auto old_thick = dc.thick;
  if (dc.flags & DCF_LOCATE_NEAREST)
    dc.nearest_dist = INT64_MAX;
  for (size_t pos = 0; pos < size;) {
    auto *elem = elems + pos;
    auto type = *elem & SPG_TYPE_MASK;
    if (type == SPT_END)
      break;
    switch (type) {
    case SPT_COLOR:
      dc.color.c1.rop &= ~ROPBF_DITHER;
      dc.color.c1.color = 0;
      dc.color.c0.color = elem[1];
      break;
    case SPT_DITHER_COLOR:
      dc.color.c0.color = elem[1];
      dc.color.c1.color = elem[2];
      dc.color.c1.rop |= ROPBF_DITHER;
      break;
    case SPT_THICK: {
      CSpriteT t;
      MemCpy(&t, elem, sizeof(t));
      dc.thick = t.thick;
      DCThickScaleCpp(&dc);
      break;
    }
    case SPT_TRANSFORM_ON:
      if (!(dc.flags & DCF_TRANSFORMATION)) {
        x -= dc.x;
        y -= dc.y;
        z -= dc.z;
      }
      dc.flags |= DCF_TRANSFORMATION;
      break;
    case SPT_TRANSFORM_OFF:
      if (dc.flags & DCF_TRANSFORMATION) {
        x += dc.x;
        y += dc.y;
        z += dc.z;
      }
      dc.flags &= ~DCF_TRANSFORMATION;
      break;
    case SPT_PLANAR_SYMMETRY: {
      CSpritePtPt pp;
      MemCpy(&pp, elem, sizeof(pp));
      if (DCSymmetry3SetCpp(&dc, pp.p.x1 + x, pp.p.y1 + y, z, pp.x2 + x,
                            pp.y2 + y, z, pp.x2 + x, pp.y2 + y, z + 1))
        dc.flags |= DCF_SYMMETRY;
      else
        dc.flags &= ~DCF_SYMMETRY;
      break;
    }
    case SPT_SHIFT:
    case SPT_PT: {
      CSpritePt pt;
      MemCpy(&pt, elem, sizeof(pt));
      if (type == SPT_SHIFT) {
        x += pt.x1;
        y += pt.y1;
      } else
        GrPlot3Cpp(&dc, pt.x1 + x, pt.y1 + y, z);
      break;
    }
    case SPT_POLYPT: {
      CSpriteNumPtU8s points;
      MemCpy(&points, elem, sizeof(points));
      SpritePolyPt(dc, x, y, z, points, elem + sizeof(points));
      break;
    }
    case SPT_LINE:
    case SPT_ARROW:
    case SPT_RECT: {
      CSpritePtPt pp;
      MemCpy(&pp, elem, sizeof(pp));
      if (type == SPT_LINE)
        GrLine3Cpp(&dc, pp.p.x1 + x, pp.p.y1 + y, z, pp.x2 + x, pp.y2 + y, z, 1,
                   0);
      else if (type == SPT_ARROW)
        GrArrow3Cpp(&dc, pp.p.x1 + x, pp.p.y1 + y, z, pp.x2 + x, pp.y2 + y, z);
      else
        GrRect3Cpp(&dc, pp.p.x1 + x, pp.p.y1 + y, z, int64_t(pp.x2) - pp.p.x1,
                   int64_t(pp.y2) - pp.p.y1);
      break;
    }
    case SPT_POLYLINE: {
      CSpriteNumU8s line;
      MemCpy(&line, elem, sizeof(line));
      SpritePolyline(dc, x, y, z, elem + sizeof(line), line.num);
      break;
    }
    case SPT_BITMAP: {
      CSpritePtWH pwh;
      MemCpy(&pwh, elem, sizeof(pwh));
      CDC img;
      MemSet(&img, 0, sizeof(img));
      img.width = pwh.width;
      img.width_internal = (pwh.width + 7) & ~7;
      img.height = pwh.height;
      img.body = const_cast<uint8_t *>(elem + sizeof(pwh));
      img.dc_signature = DCS_SIGNATURE_VAL;
      GrBlot3Cpp(&dc, pwh.p.x1 + x, pwh.p.y1 + y, z, &img);
      break;
    }
    }
    pos += SpriteElemSize(elem, size - pos);
  }
  dc.sym = old_sym;
  dc.thick = old_thick;
  dc.color = old_color;
  dc.flags = (dc.flags & ~flags_3d) | (old_flags & flags_3d);
  return true;
}

extern "C" void Sprite3Cpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                           const uint8_t *elems, uint8_t just_one_elem)
{
  if (!dc)
    dc = gr.dc;
  if (dc)
    Sprite3(*dc, x, y, z, elems, INT64_MAX, just_one_elem);
}

extern "C" void Sprite3BCpp(CDC *dc, int64_t x, int64_t y, int64_t z,
                            const uint8_t *elems)
{
  if (!dc)
    dc = gr.dc;
  if (!dc)
    return;
  auto old_x = dc->x, old_y = dc->y, old_z = dc->z;
  auto old_flags = dc->flags;
  dc->x = x;
  dc->y = y;
  dc->z = z;
  dc->flags |= DCF_TRANSFORMATION;
  Sprite3(*dc, 0, 0, 0, elems, INT64_MAX);
  dc->x = old_x;
  dc->y = old_y;
  dc->z = old_z;
  dc->flags =
      (dc->flags & ~DCF_TRANSFORMATION) | (old_flags & DCF_TRANSFORMATION);
}

extern "C" CDocEntryBase *DocSpriteCpp(CDoc *doc, const uint8_t *elems,
                                       const char *fmt)
{
  if (!doc)
    doc = DocPut();
  if (!doc || doc->doc_signature != DOC_SIGNATURE_VAL)
    return nullptr;
  auto size = SpriteSize(elems, INT64_MAX);
  if (!size || size > UINT32_MAX)
    return nullptr;
  CDocLock lock(*doc);
  if (doc->cur_bin_num >= INT32_MAX)
    return nullptr;
  auto *heap = MemTaskHeap(doc->mem_task);
  auto *bin = reinterpret_cast<CDocBin *>(CAlloc(sizeof(CDocBin), heap));
  if (!bin)
    return nullptr;
  bin->data = MAlloc(size, heap);
  if (!bin->data) {
    Free(bin);
    return nullptr;
  }
  bin->num = doc->cur_bin_num++;
  const uint64_t arg = bin->num;
  auto *text = StrPrintJoin(nullptr, fmt ? fmt : "$SP,\"\",BI=%d$", 1, &arg);
  auto *base = text ? DocPutS(*doc, text, StrLen(text)) : nullptr;
  Free(text);
  auto *entry = reinterpret_cast<CDocEntry *>(base);
  if (!base || !(base->de_flags & DOCEF_HAS_BIN) || entry->bin_data ||
      uint32_t(entry->bin_num) != bin->num) {
    Free(bin->data);
    Free(bin);
    return nullptr;
  }
  MemCpy(bin->data, elems, size);
  bin->size = size;
  bin->use_cnt = 1;
  if (base->tag && *base->tag && (base->de_flags & DOCEF_TAG))
    bin->tag = StrNew(base->tag, heap);
  QueIns(*bin, *doc->bin_head.last);
  entry->bin_data = bin;
  return base;
}

extern "C" CDocEntryBase *SpriteCpp(const uint8_t *elems, const char *fmt)
{
  return DocSpriteCpp(nullptr, elems, fmt);
}
