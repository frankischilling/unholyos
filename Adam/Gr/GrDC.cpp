#include "../../Kernel/Compress.hpp"
#include "../../Kernel/KernelB.hpp"
#include "GrGlbls.hpp"
#include "Gr.hpp"

extern "C" void DCTransformHC(CDC *dc, int64_t *x, int64_t *y, int64_t *z);
extern "C" void DCLightingHC(CDC *dc, CD3I32 *p1, CD3I32 *p2, CD3I32 *p3,
                             uint32_t color);
extern "C" int64_t *Mat4x4NewCpp(const int64_t *src, CTask *mem_task);

constexpr uint64_t GRAPHICS_EXCEPTION = 0x7363696870617247;
constexpr size_t DC_HEADER_SIZE = offsetof(CDC, palette);

static size_t DCBodySize(const CDC *dc)
{
  return static_cast<size_t>(dc->width_internal) * dc->height;
}

extern "C" void Mat4x4MulXYZCpp(const int64_t *r, int64_t *_x, int64_t *_y,
                                int64_t *_z)
{
  uint64_t xx = *_x, yy = *_y, zz = *_z;
  int64_t x1 =
      static_cast<int64_t>(r[0] * xx + r[1] * yy + r[2] * zz + r[3]) >> 32;
  int64_t y1 =
      static_cast<int64_t>(r[4] * xx + r[5] * yy + r[6] * zz + r[7]) >> 32;
  int64_t z1 =
      static_cast<int64_t>(r[8] * xx + r[9] * yy + r[10] * zz + r[11]) >> 32;
  *_x = x1;
  *_y = y1;
  *_z = z1;
}

extern "C" void DCTransformCpp(CDC *dc, int64_t *_x, int64_t *_y, int64_t *_z)
{
  Mat4x4MulXYZCpp(dc->r, _x, _y, _z);
  *_x = static_cast<uint64_t>(*_x) + dc->x;
  *_y = static_cast<uint64_t>(*_y) + dc->y;
  *_z = static_cast<uint64_t>(*_z) + dc->z;
}

extern "C" int64_t *Mat4x4IdentEquCpp(int64_t *r)
{
  MemSet(r, 0, 16 * sizeof(*r));
  r[0] = r[5] = r[10] = r[15] = GR_SCALE;
  return r;
}

extern "C" int64_t *Mat4x4IdentNewCpp(CTask *mem_task)
{
  auto *r =
      reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), mem_task));
  return r ? Mat4x4IdentEquCpp(r) : nullptr;
}

static uint64_t SqrI64(uint64_t value)
{
  return value * value;
}

extern "C" int64_t Mat4x4NormSqr65536Cpp(const int64_t *r)
{
  auto sum = [](const int64_t *p, uint64_t scale) {
    return static_cast<int64_t>(p[0] * scale + p[1] * scale + p[2] * scale) >>
           32;
  };
  return SqrI64(sum(r, 37838)) + SqrI64(sum(r + 4, 37837)) +
         SqrI64(sum(r + 8, 37837));
}

extern "C" void DCMat4x4SetCpp(CDC *dc, int64_t *r)
{
  if (!dc)
    dc = gr.dc;
  dc->r = r;
  auto norm = F64SqrtBits(F64FromI64(Mat4x4NormSqr65536Cpp(r)));
  dc->r_norm = F64ToI64(F64MulBits(norm, F64FromI64(65536)), 0x0C00);
}

extern "C" void DCLightingCpp(CDC *dc, CD3I32 *p1, CD3I32 *p2, CD3I32 *p3,
                              uint32_t color)
{
  CD3I32 v1{static_cast<int32_t>(static_cast<uint32_t>(p1->x) - p2->x),
            static_cast<int32_t>(static_cast<uint32_t>(p1->y) - p2->y),
            static_cast<int32_t>(static_cast<uint32_t>(p1->z) - p2->z)};
  CD3I32 v2{static_cast<int32_t>(static_cast<uint32_t>(p3->x) - p2->x),
            static_cast<int32_t>(static_cast<uint32_t>(p3->y) - p2->y),
            static_cast<int32_t>(static_cast<uint32_t>(p3->z) - p2->z)};
  int64_t vn_x =
      static_cast<uint64_t>(v1.y) * v2.z - static_cast<uint64_t>(v1.z) * v2.y;
  int64_t vn_y =
      static_cast<uint64_t>(v1.z) * v2.x - static_cast<uint64_t>(v1.x) * v2.z;
  int64_t vn_z =
      static_cast<uint64_t>(v1.x) * v2.y - static_cast<uint64_t>(v1.y) * v2.x;
  auto d = F64SqrtBits(F64FromI64(SqrI64(vn_x) + SqrI64(vn_y) + SqrI64(vn_z)));
  if (d)
    d = F64DivBits(F64FromI64(65536), d);
  auto scale = [d](int64_t n) {
    return static_cast<uint64_t>(
        F64ToI64(F64MulBits(F64FromI64(n), d), 0x0C00));
  };
  int64_t i =
      static_cast<int64_t>(scale(vn_x) * dc->ls.x + scale(vn_y) * dc->ls.y +
                           scale(vn_z) * dc->ls.z) >>
      16;
  if (color & ROPF_TWO_SIDED) {
    color &= ~ROPF_TWO_SIDED;
    i = (i < 0 ? -static_cast<uint64_t>(i) : static_cast<uint64_t>(i)) * 2;
  } else
    i += 65536;
  if (color & ROPF_HALF_RANGE_COLOR) {
    color &= ~ROPF_HALF_RANGE_COLOR;
    i >>= 1;
    if (color >= 8) {
      color -= 8;
      i += 65536;
    }
  }
  if (i < 65536) {
    dc->color = __builtin_bit_cast(CColorROPU32, ROPF_PROBABILITY_DITHER +
                                                     (color << 16) + BLACK);
    dc->dither_probability_u16 = i;
  } else {
    dc->color = __builtin_bit_cast(
        CColorROPU32, ROPF_PROBABILITY_DITHER + ((color ^ 8) << 16) + color);
    dc->dither_probability_u16 = i - 65536;
  }
}

extern "C" void DCFillCpp(CDC *dc, uint32_t val)
{
  if (!dc)
    dc = gr.dc;
  if (dc->body)
    MemSet(dc->body, val, DCBodySize(dc));
}

extern "C" void DCClearCpp(CDC *dc)
{
  DCFillCpp(dc, BLACK);
}

extern "C" void DCRstCpp(CDC *dc)
{
  dc->color = {};
  dc->color2 = {};
  dc->bkcolor = {};
  dc->collision_cnt = 0;
  dc->thick = 1;
  dc->ls = {37837, 37837, 37837};
  dc->x = dc->y = dc->z = 0;
  dc->transform = DCTransformHC;
  dc->lighting = DCLightingHC;
  Mat4x4IdentEquCpp(dc->r);
  dc->r_norm = GR_SCALE;
  dc->flags &= ~(DCF_SYMMETRY | DCF_TRANSFORMATION | DCF_JUST_MIRROR);
  MemCpy(dc->palette, gr_palette_std, sizeof(dc->palette));
}

extern "C" void DCExtentsInitCpp(CDC *dc)
{
  if (!dc)
    dc = gr.dc;
  dc->flags |= DCF_RECORD_EXTENTS;
  dc->min_x = INT64_MAX;
  dc->max_x = INT64_MIN;
  dc->min_y = INT64_MAX;
  dc->max_y = INT64_MIN;
}

extern "C" CDC *DCAliasCpp(CDC *dc, CTask *task)
{
  if (!dc)
    dc = gr.dc;
  if (!task)
    task = CurrentTask();
  if (dc->dc_signature != DCS_SIGNATURE_VAL)
    RaiseHolyC(GRAPHICS_EXCEPTION);
  auto *res = reinterpret_cast<CDC *>(MAllocCpp(sizeof(CDC), task));
  if (!res)
    return nullptr;
  auto *r = reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), task));
  if (!r) {
    Free(res);
    return nullptr;
  }
  MemCpy(res, dc, sizeof(CDC));
  res->win_task = res->mem_task = task;
  res->r = r;
  DCRstCpp(res);
  res->flags |= DCF_ALIAS;
  res->alias = dc;
  return res;
}

extern "C" CDC *DCNewCpp(int64_t width, int64_t height, CTask *task,
                         uint8_t null_bitmap)
{
  if (width < 0 || width > INT32_MAX - 7 || height < 0 || height > INT32_MAX)
    return nullptr;
  if (!task)
    task = CurrentTask();
  auto *res = reinterpret_cast<CDC *>(CAllocCpp(sizeof(CDC), task));
  if (!res)
    return nullptr;
  res->win_task = res->mem_task = task;
  res->width = width;
  res->width_internal = (width + 7) & ~7;
  res->height = height;
  if (null_bitmap)
    res->flags |= DCF_DONT_DRAW;
  else
    res->body = CAllocCpp(DCBodySize(res), task);
  res->r = reinterpret_cast<int64_t *>(MAllocCpp(16 * sizeof(int64_t), task));
  res->dc_signature = DCS_SIGNATURE_VAL;
  if (!res->r || (!null_bitmap && !res->body)) {
    DCDelCpp(res);
    return nullptr;
  }
  DCRstCpp(res);
  return res;
}

extern "C" void DCDelCpp(CDC *dc)
{
  if (!dc)
    return;
  if (dc->dc_signature != DCS_SIGNATURE_VAL)
    RaiseHolyC(GRAPHICS_EXCEPTION);
  dc->dc_signature = 0;
  Free(dc->r);
  if (!(dc->flags & DCF_ALIAS))
    Free(dc->body);
  Free(dc->depth_buf);
  Free(dc);
}

extern "C" int64_t DCSizeCpp(CDC *dc)
{
  return dc ? MSize2(dc) + MSize2(dc->body) + MSize2(dc->depth_buf) : 0;
}

extern "C" int32_t *DCDepthBufRstCpp(CDC *dc)
{
  if (dc->depth_buf)
    for (size_t i = 0, size = DCBodySize(dc); i < size; i++)
      dc->depth_buf[i] = INT32_MAX;
  return dc->depth_buf;
}

extern "C" int32_t *DCDepthBufAllocCpp(CDC *dc)
{
  auto *depth = reinterpret_cast<int32_t *>(
      MAllocCpp(DCBodySize(dc) * sizeof(int32_t), dc->mem_task));
  if (!depth)
    return nullptr;
  Free(dc->depth_buf);
  dc->depth_buf = depth;
  return DCDepthBufRstCpp(dc);
}

extern "C" CDC *DCCopyCpp(CDC *dc, CTask *task)
{
  if (!dc)
    return nullptr;
  if (dc->dc_signature != DCS_SIGNATURE_VAL)
    RaiseHolyC(GRAPHICS_EXCEPTION);
  auto *res = reinterpret_cast<CDC *>(MAllocIdentCpp(dc, task));
  if (!res)
    return nullptr;
  auto *r = Mat4x4NewCpp(dc->r, task);
  auto *body = MAllocIdentCpp(dc->body, task);
  auto *depth =
      reinterpret_cast<int32_t *>(MAllocIdentCpp(dc->depth_buf, task));
  if (!r || (dc->body && !body) || (dc->depth_buf && !depth)) {
    Free(r);
    Free(body);
    Free(depth);
    Free(res);
    return nullptr;
  }
  DCMat4x4SetCpp(res, r);
  res->mem_task = task;
  res->body = body;
  res->depth_buf = depth;
  return res;
}

extern "C" void DCMonoCpp(CDC *dc, int64_t quest, int64_t true_color,
                          int64_t false_color)
{
  for (size_t i = 0, size = DCBodySize(dc); dc->body && i < size; i++)
    dc->body[i] = dc->body[i] == quest ? true_color : false_color;
}

extern "C" int64_t DCColorChgCpp(CDC *dc, int64_t src_color, int64_t dst_color)
{
  int64_t res = 0;
  for (size_t i = 0, size = DCBodySize(dc); dc->body && i < size; i++)
    if (dc->body[i] == src_color) {
      dc->body[i] = dst_color;
      res++;
    }
  return res;
}

extern "C" uint8_t *DCSaveCpp(CDC *dc, int64_t *_size, int64_t dcsf_flags)
{
  if (_size)
    *_size = 0;
  if (dc->width_internal < 0 || dc->height < 0)
    return nullptr;
  size_t body_size = DCBodySize(dc);
  if (!dc->body && body_size)
    return nullptr;
  auto *hc = CurrentTask()->data_heap;
  uint8_t *arc = nullptr;
  const uint8_t *body = dc->body;
  uint32_t flags = 0;
  if (dcsf_flags & DCSF_COMPRESSED) {
    if (body_size > SIZE_MAX / 8 - ARC_WRITE_OVERHEAD)
      return nullptr;
    size_t capacity = body_size + ARC_WRITE_OVERHEAD;
    arc = MAlloc(capacity, hc);
    auto *ctrl =
        reinterpret_cast<CArcEncodeCtrl *>(MAlloc(sizeof(CArcEncodeCtrl), hc));
    body_size =
        arc && ctrl ? ArcCompressBuf(body, body_size, arc, capacity, *ctrl) : 0;
    Free(ctrl);
    if (!body_size) {
      Free(arc);
      return nullptr;
    }
    body = arc;
    flags = DCF_COMPRESSED;
  }
  CBGR48 palette[COLORS_NUM];
  if (dcsf_flags & DCSF_PALETTE_GET)
    GrPaletteGetCpp(palette);
  else
    MemCpy(palette, dc->palette, sizeof(palette));
  for (unsigned i = 0; i < COLORS_NUM; i++)
    if (__builtin_bit_cast(uint64_t, palette[i]) !=
        __builtin_bit_cast(uint64_t, gr_palette_std[i])) {
      flags |= DCF_PALETTE;
      break;
    }
  size_t palette_size = flags & DCF_PALETTE ? sizeof(palette) : 0;
  size_t total_size = DC_HEADER_SIZE + palette_size + body_size;
  auto *res = MAlloc(total_size, hc);
  if (res) {
    MemCpy(res, dc, DC_HEADER_SIZE);
    MemCpy(res + offsetof(CDC, flags), &flags, sizeof(flags));
    MemCpy(res + DC_HEADER_SIZE, palette, palette_size);
    MemCpy(res + DC_HEADER_SIZE + palette_size, body, body_size);
    if (_size)
      *_size = total_size;
  }
  Free(arc);
  return res;
}

static CDC *DCLoad(const uint8_t *src, size_t available, int64_t *_size,
                   CTask *task)
{
  if (_size)
    *_size = 0;
  if (!src || available < DC_HEADER_SIZE)
    return nullptr;
  if (!task)
    task = CurrentTask();
  auto *res = reinterpret_cast<CDC *>(CAllocCpp(sizeof(CDC), task));
  if (!res)
    return nullptr;
  res->win_task = res->mem_task = task;
  res->dc_signature = DCS_SIGNATURE_VAL;
  MemCpy(res, src, DC_HEADER_SIZE);
  size_t extra = res->flags & DCF_PALETTE ? sizeof(res->palette) : 0;
  size_t prefix = DC_HEADER_SIZE + extra;
  if (prefix > available || res->width < 0 || res->height < 0 ||
      res->width_internal < res->width || res->width_internal % 8) {
    Free(res);
    return nullptr;
  }
  MemCpy(res->palette,
         extra ? src + DC_HEADER_SIZE
               : reinterpret_cast<const uint8_t *>(gr_palette_std),
         sizeof(res->palette));
  const auto *body = src + prefix;
  size_t body_size = DCBodySize(res), stored = body_size;
  bool compressed = res->flags & DCF_COMPRESSED;
  CArcCompress arc{};
  if (compressed) {
    if (!ArcReadHeader(body, available - prefix, arc) ||
        arc.expanded_size != body_size) {
      Free(res);
      return nullptr;
    }
    stored = arc.compressed_size;
  } else if (stored > available - prefix) {
    Free(res);
    return nullptr;
  }

  res->body = MAllocCpp(body_size + compressed, task);
  res->r = Mat4x4IdentNewCpp(task);
  if (res->body && res->r) {
    bool okay = true;
    if (compressed) {
      auto *ctrl = arc.compression_type != CT_NONE
                       ? reinterpret_cast<CArcCtrl *>(
                             MAlloc(sizeof(CArcCtrl), CurrentTask()->data_heap))
                       : nullptr;
      okay = ArcExpandBuf(body, stored, res->body, body_size, ctrl);
      Free(ctrl);
      res->body[body_size] = 0;
      res->flags &= ~DCF_COMPRESSED;
    } else
      MemCpy(res->body, body, body_size);
    if (okay) {
      res->thick = 1;
      res->r_norm = GR_SCALE;
      if (_size)
        *_size = prefix + stored;
      return res;
    }
  }
  // The file flags are untrusted, and the allocations above belong to res.
  res->flags &= ~DCF_ALIAS;
  DCDelCpp(res);
  return nullptr;
}

extern "C" CDC *DCLoadCpp(const uint8_t *src, int64_t *_size, CTask *task)
{
  return DCLoad(src, SIZE_MAX, _size, task);
}

extern "C" int64_t GRWriteCpp(const char *filename, CDC *dc, int64_t dcsf_flags)
{
  int64_t size;
  auto *st = ExtDftCpp(filename, "GR.Z");
  auto *src = DCSaveCpp(dc, &size, dcsf_flags);
  if (st && src)
    FileWriteCpp(st, src, size, 0, 0);
  Free(st);
  Free(src);
  return size;
}

extern "C" CDC *GRReadCpp(const char *filename, CTask *task)
{
  size_t size;
  auto *st = ExtDftCpp(filename, "GR.Z");
  auto *src = st ? FileReadCpp(st, &size, nullptr) : nullptr;
  auto *dc = src ? DCLoad(src, size, nullptr, task) : nullptr;
  Free(src);
  Free(st);
  return dc;
}

extern "C" void DocGRCpp(CDoc *doc,const char *filename)
{
  CDC *dc=GRReadCpp(filename,nullptr);
  if (dc) {
    uint8_t *elems=DC2SpriteCpp(dc);
    if (elems)
      DocSpriteCpp(doc,elems,nullptr);
    Free(elems);
    DCDelCpp(dc);
  }
}
