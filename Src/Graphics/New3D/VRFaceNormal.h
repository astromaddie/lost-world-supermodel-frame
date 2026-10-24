#ifndef NEW3D_VR_FACE_NORMAL_H
#define NEW3D_VR_FACE_NORMAL_H

#include <cmath>

namespace New3D {

// Correct only the viewport remap A, not Model 3's original normal transform B.
// The shader currently has n = k R A B n0 (R = rigid eye rotation, k = cinema scale).
// C = R A^-T A^-1 R^T gives C n = k R A^-T B n0. Keeping k preserves the
// existing k^2 plane-dot contribution to LOD, cancelled by gl_FragCoord.w^2.
// A has rows [kx,0,sx], [0,ky,sy], [0,0,1]. All matrices are column-major.
inline bool VRFaceNormalCorrection(const float remap[16], const float eye[16], float out[9])
{
  const float kx = remap[0], ky = remap[5], sx = remap[8], sy = remap[9];
  if (kx == 0.f || ky == 0.f) return false;
  const float ix = 1.f / kx, iy = 1.f / ky;
  const float xx = ix * ix, yy = iy * iy;
  const float q[9] = {xx, 0, -sx*xx, 0, yy, -sy*yy,
                     -sx*xx, -sy*yy, 1.f + sx*sx*xx + sy*sy*yy};
  for (int c = 0; c < 3; ++c) {
    for (int r = 0; r < 3; ++r) {
      float value = 0;
      for (int j = 0; j < 3; ++j)
        for (int k = 0; k < 3; ++k)
          value += eye[j*4+r] * q[k*3+j] * eye[k*4+c];
      if (!std::isfinite(value)) return false; // retain legacy handling of degenerate input
      out[c*3+r] = value;
    }
  }
  return true;
}

} // namespace New3D
#endif
