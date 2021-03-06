// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FPDF_PAGE_CPDF_MESHSTREAM_H_
#define CORE_FPDFAPI_FPDF_PAGE_CPDF_MESHSTREAM_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/fpdf_page/cpdf_shadingpattern.h"
#include "core/fpdfapi/fpdf_parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_system.h"

struct CPDF_MeshVertex {
  FX_FLOAT x;
  FX_FLOAT y;
  FX_FLOAT r;
  FX_FLOAT g;
  FX_FLOAT b;
};

class CFX_Matrix;
class CPDF_ColorSpace;
class CPDF_Function;
class CPDF_Stream;

class CPDF_MeshStream {
 public:
  CPDF_MeshStream(ShadingType type,
                  const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
                  CPDF_Stream* pShadingStream,
                  CPDF_ColorSpace* pCS);

  bool Load();

  uint32_t GetFlag();
  void GetCoords(FX_FLOAT& x, FX_FLOAT& y);
  void GetColor(FX_FLOAT& r, FX_FLOAT& g, FX_FLOAT& b);

  uint32_t GetVertex(CPDF_MeshVertex& vertex, CFX_Matrix* pObject2Bitmap);
  FX_BOOL GetVertexRow(CPDF_MeshVertex* vertex,
                       int count,
                       CFX_Matrix* pObject2Bitmap);

  CFX_BitStream* BitStream() { return &m_BitStream; }
  uint32_t ComponentBits() const { return m_nComponentBits; }
  uint32_t Components() const { return m_nComponents; }

 private:
  static const uint32_t kMaxComponents = 8;

  const ShadingType m_type;
  const std::vector<std::unique_ptr<CPDF_Function>>& m_funcs;
  CPDF_Stream* const m_pShadingStream;
  CPDF_ColorSpace* const m_pCS;
  uint32_t m_nCoordBits;
  uint32_t m_nComponentBits;
  uint32_t m_nFlagBits;
  uint32_t m_nComponents;
  uint32_t m_CoordMax;
  uint32_t m_ComponentMax;
  FX_FLOAT m_xmin;
  FX_FLOAT m_xmax;
  FX_FLOAT m_ymin;
  FX_FLOAT m_ymax;
  FX_FLOAT m_ColorMin[kMaxComponents];
  FX_FLOAT m_ColorMax[kMaxComponents];
  CPDF_StreamAcc m_Stream;
  CFX_BitStream m_BitStream;
};

#endif  // CORE_FPDFAPI_FPDF_PAGE_CPDF_MESHSTREAM_H_
