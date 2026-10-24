#include <cassert>
#include <cstdio>
#include "../../Src/OSD/VR/DiagnosticViewLock.h"

static bool Same(const XrFovf& a, const XrFovf& b)
{
  return a.angleLeft==b.angleLeft && a.angleRight==b.angleRight &&
         a.angleUp==b.angleUp && a.angleDown==b.angleDown;
}
int main()
{
  XrView views[2]{};
  views[0].fov={-.9f,.7f,.8f,-.6f};
  views[1].fov={-.65f,.95f,.75f,-.7f};
  const XrFovf initial[2]={views[0].fov,views[1].fov};
  float ipd=.064f;
  DiagnosticViewLock lock;
  lock.Apply(false,views,ipd);
  assert(!lock.locked && Same(views[0].fov,initial[0]) && ipd==.064f);
  lock.Apply(true,views,ipd);
  assert(lock.locked);
  for(int frame=0;frame<8;++frame) {
    views[0].fov={-.5f,.6f,.9f,-1.f}; views[1].fov={-.8f,.9f,.6f,-.5f}; ipd=.07f;
    lock.Apply(true,views,ipd);
    assert(Same(views[0].fov,initial[0]) && Same(views[1].fov,initial[1]) && ipd==.064f);
    // Submission copies the same XrViews that ComputeEye reads; no parallel projection state.
    XrCompositionLayerProjectionView submitted[2]{};
    for(int eye=0;eye<2;++eye) { submitted[eye].fov=views[eye].fov; assert(Same(submitted[eye].fov,initial[eye])); }
  }
  views[0].fov={-.5f,.6f,.9f,-1.f}; ipd=.07f;
  const XrFovf live=views[0].fov;
  lock.Apply(false,views,ipd);
  assert(Same(views[0].fov,live) && ipd==.07f);
  std::puts("PASS: held diagnostic FOV/IPD lock, asymmetric eyes, 8 changing runtime views, live bypass");
}
