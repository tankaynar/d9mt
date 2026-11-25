#pragma once
#include <cstring>

namespace dxmt {

// Which states a state block holds. D3D9: CreateStateBlock(ALL) holds everything; a block recorded with
// Begin/EndStateBlock holds exactly the states set while recording (and recording does not change the device).
struct D3D9StateMask {
  bool rs[256], tss[8][33], samp[16][14], xf[512], light[32], light_en[32], tex[16], stream[16], vsc[256], psc[256];
  bool viewport, scissor, material, fvf, vs, ps, vdecl, ib;
  void setAll() { memset(this, 1, sizeof(*this)); }
};

} // namespace dxmt
