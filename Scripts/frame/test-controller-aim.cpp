#include "../../Src/OSD/VR/ControllerAim.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static bool Near(float a, float b) { return std::fabs(a-b) < 0.00002f; }
static XrVector3f Rotate(const XrQuaternionf& q, XrVector3f v)
{
  // Independent reference: Hamilton sandwich, rather than the production basis formula.
  const auto p = ControllerAim::Multiply(ControllerAim::Multiply(q, {v.x,v.y,v.z,0}), {-q.x,-q.y,-q.z,q.w});
  return {p.x,p.y,p.z};
}
static void TestPoseAndRay()
{
  const XrQuaternionf identity{0,0,0,1};
  assert(Near(ControllerAim::Forward(identity).z, -1));
  for (float yaw : {-120.f, -35.f, 0.f, 48.f, 170.f})
   for (float pitch : {-70.f, 0.f, 45.f})
    for (float roll : {-90.f, 0.f, 30.f, 180.f}) {
      const float h=roll*0.008726646259971648f;
      const auto base=ControllerAim::Multiply(ControllerAim::Correct(identity,yaw,pitch),{0,0,std::sin(h),std::cos(h)});
      for (float nudge : {-3.f,0.f,3.f}) {
        const auto gun=ControllerAim::Correct(base,nudge,-nudge);
        const auto ray=ControllerAim::Forward(gun);
        const auto muzzle=Rotate(gun,{0,0,-0.18f});
        const auto bore=Rotate(gun,{0,0,-0.28f});
        assert(Near((bore.x-muzzle.x)/0.1f,ray.x));
        assert(Near((bore.y-muzzle.y)/0.1f,ray.y));
        assert(Near((bore.z-muzzle.z)/0.1f,ray.z));
        assert(Near(ray.x*ray.x+ray.y*ray.y+ray.z*ray.z,1));
      }
    }
  // Adjustment sign and controller roll: right stays hand-local, even when inverted.
  auto d=ControllerAim::Forward(ControllerAim::Correct(identity,3,3));
  assert(d.x>0 && d.y>0);
  d=ControllerAim::Forward(ControllerAim::Correct({0,0,1,0},3,3));
  assert(d.x<0 && d.y<0);
}
static void TestProjection()
{
  for (float depth : {2.f,20.f})
   for (float scale : {0.55f,1.f})
    for (float u : {-0.5f,0.f,0.5f,1.f,1.5f})
     for (float v : {0.f,0.5f,1.f})
      for (bool positional : {false,true}) {
        const XrVector3f head{0.7f,0.4f,-0.3f}, hand{0.95f,0.25f,-0.7f};
        const auto o=ControllerAim::SceneOrigin(hand,head,positional);
        // Asymmetric reference frustum catches swapped axes/signs and hidden center offsets.
        const XrVector3f target{(-0.7f+u*1.8f)*depth*scale,(0.9f-v*1.5f)*depth*scale,-depth};
        XrVector3f ray{target.x-o.x,target.y-o.y,target.z-o.z};
        float length=std::sqrt(ray.x*ray.x+ray.y*ray.y+ray.z*ray.z);
        ray={ray.x/length,ray.y/length,ray.z/length};
        float actualU,actualV;
        assert(ControllerAim::Project(o,ray,depth,scale,-0.7f,1.1f,0.9f,-0.6f,actualU,actualV));
        assert(Near(u,actualU) && Near(v,actualV));
        // Starting on the same ray at its visible muzzle must hit exactly the same point.
        const XrVector3f muzzle{o.x+ray.x*0.18f,o.y+ray.y*0.18f,o.z+ray.z*0.18f};
        assert(ControllerAim::Project(muzzle,ray,depth,scale,-0.7f,1.1f,0.9f,-0.6f,actualU,actualV));
        assert(Near(u,actualU) && Near(v,actualV));
      }
  float u,v;
  assert(!ControllerAim::Project({0,0,0},{0,0,1},20,1,-1,1,1,-1,u,v));
  assert(!ControllerAim::Project({0,0,-30},{0,0,-1},20,1,-1,1,1,-1,u,v));
  assert(!ControllerAim::Project({0,0,0},{1,0,0},20,1,-1,1,1,-1,u,v));
}
static void TestFrustumChange()
{
  const XrVector3f origin{0.25f,-0.15f,-0.4f};
  for (float x : {-0.7f,0.f,0.3f,1.1f})
   for (float y : {-0.6f,0.f,0.9f}) {
    const XrVector3f direction{x-origin.x/20.f,y-origin.y/20.f,-1.f-origin.z/20.f};
    float oldU,oldV;
    assert(ControllerAim::Project(origin,direction,20,1,-0.5f,0.5f,0.5f,-0.5f,oldU,oldV));
    const float inputU=oldU,inputV=oldV; // The emulator has consumed this snapshot.
    float u,v;
    assert(ControllerAim::ProjectPointer(origin,direction,20,1,-0.7f,1.1f,0.9f,-0.6f,true,u,v));
    assert(Near(-0.7f+u*1.8f,x) && Near(0.9f-v*1.5f,y));
    assert(oldU==inputU && oldV==inputV);
    // Reconstructing old input against the new frustum must NOT be the display path.
    assert(!Near(-0.7f+inputU*1.8f,x) || !Near(0.9f-inputV*1.5f,y));
  }
  // Current-frustum clipping: an old offscreen point becomes visible after zoom-out.
  const XrVector3f ray{0.9f,0,-1};
  float u,v;
  assert(!ControllerAim::ProjectPointer({0,0,0},ray,20,1,-0.4f,0.4f,0.5f,-0.5f,false,u,v));
  assert(ControllerAim::ProjectPointer({0,0,0},ray,20,1,-0.7f,1.1f,0.9f,-0.6f,false,u,v));
  // A cinema size change uses the new scale and depth, with the same stored ray.
  assert(ControllerAim::ProjectPointer({0.2f,-0.1f,-0.4f},{0.1f,0.05f,-1},2,0.75f,-0.7f,1.1f,0.9f,-0.6f,false,u,v));
  assert(Near((-0.7f+u*1.8f)*1.5f,0.36f));
  assert(Near((0.9f-v*1.5f)*1.5f,-0.02f));
  // Extended in-eye rings and clipped compositor HUD intentionally have different bounds.
  assert(ControllerAim::ProjectPointer({0,0,0},{1.5f,0,-1},20,1,-1,1,1,-1,true,u,v));
  assert(!ControllerAim::ProjectPointer({0,0,0},{1.5f,0,-1},20,1,-1,1,1,-1,false,u,v));
}
static void TestLostWorldCalibration()
{
  const auto wide=ControllerAim::LostWorldMapping(true), native=ControllerAim::LostWorldMapping(false);
  // X follows the production input-path center regression; Y retains the PC measurements.
  const float screen[]={0,0.5f,1};
  const float gunX[]={0.22515f,0.5f,0.77485f},gunY[]={0.2155f,0.49085f,0.7662f};
  for(int i=0;i<3;i++) {
    assert(Near(screen[i]*wide.scaleX+wide.offsetX,gunX[i]));
    assert(Near(screen[i]*wide.scaleY+wide.offsetY,gunY[i]));
    assert(Near(screen[i]*native.scaleX+native.offsetX,screen[i]));
    assert(Near(screen[i]*native.scaleY+native.offsetY,screen[i]));
  }
}
int main()
{
  TestPoseAndRay(); TestProjection(); TestFrustumChange(); TestLostWorldCalibration();
  std::puts("Controller aim: pose/barrel/ray, rotated hands, plane edges/depth, positional modes, changing frusta/cinema/clipping, wide calibration passed");
}
