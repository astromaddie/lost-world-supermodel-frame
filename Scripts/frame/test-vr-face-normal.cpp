#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include "../../Src/Graphics/New3D/VRFaceNormal.h"

static void Mul3(const float m[9], const float v[3], float out[3])
{ for (int r=0;r<3;++r) out[r]=m[r]*v[0]+m[3+r]*v[1]+m[6+r]*v[2]; }
static void Rotate(const float m[16], const float v[3], float out[3])
{ for (int r=0;r<3;++r) out[r]=m[r]*v[0]+m[4+r]*v[1]+m[8+r]*v[2]; }
static float Dot(const float a[3],const float b[3])
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
int main()
{
  float a[16]={2,0,0,0, 0,.75f,0,0, -.2f,.15f,1,0, 0,0,0,1};
  const float c=std::cos(.37f),s=std::sin(.37f);
  float eye[16]={c,0,-s,0, 0,1,0,0, s,0,c,0, .05f,0,0,1};
  float correction[9];
  assert(New3D::VRFaceNormalCorrection(a,eye,correction));
  const float normal[3]={1,0,1};
  float an[3],ran[3],fixed[3];
  Rotate(a,normal,an); Rotate(eye,an,ran); Mul3(correction,ran,fixed);
  // Plane x+z=-2. Original face-dot is constant; legacy affine normals cut its interior.
  float minimum=100,maximum=-100;
  for(float x : {-.25f,.25f,.75f,1.f}) {
    const float point[3]={x,.1f,-2-x};
    float ap[3],rap[3]; Rotate(a,point,ap); Rotate(eye,ap,rap);
    assert(std::fabs(Dot(rap,fixed)+2.f)<1e-5f);
    const float legacy=Dot(rap,ran);
    minimum=std::fmin(minimum,legacy); maximum=std::fmax(maximum,legacy);
    // Cinema scale k remains in both uploaded geometry and its legacy normal.
    // The resulting k^2 factor cancels perspective w^2 in the existing LOD expression.
    const float baseline=Dot(rap,fixed)/(rap[2]*rap[2]);
    for(float k : {.5f,2.f,4.f}) {
      float p[3],n[3],cn[3];
      for(int i=0;i<3;++i) {p[i]=k*rap[i];n[i]=k*ran[i];}
      Mul3(correction,n,cn);
      assert(std::fabs(Dot(p,cn)/(p[2]*p[2])-baseline)<1e-5f);
    }
  }
  assert(minimum<0 && maximum>0);
  // Arbitrary legacy B-normal is preserved; it is never replaced with inverse(B)^T.
  const float legacyBNormal[3]={3,.4f,.7f};
  Rotate(a,legacyBNormal,an); Rotate(eye,an,ran); Mul3(correction,ran,fixed);
  const float expectedPreEye[3]={legacyBNormal[0]/a[0],legacyBNormal[1]/a[5],
    legacyBNormal[2]-a[8]*legacyBNormal[0]/a[0]-a[9]*legacyBNormal[1]/a[5]};
  float expected[3]; Rotate(eye,expectedPreEye,expected);
  for(int i=0;i<3;++i) assert(std::fabs(expected[i]-fixed[i])<1e-5f);
  a[0]=0; assert(!New3D::VRFaceNormalCorrection(a,eye,correction));
  std::puts("PASS: VR-only affine face correction, planar sign, eye rotation, legacy B, cinema LOD scale");
}
